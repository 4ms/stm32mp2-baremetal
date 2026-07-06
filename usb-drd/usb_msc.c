/*
 * usb_msc.c — Bare-metal USB Mass Storage host driver (Bulk-Only Transport)
 */

#include "usb_msc.h"
#include <common.h>
#include <usb.h>
#include <string.h>

/* Mass Storage subclass / protocol (USB_CLASS_MASS_STORAGE is in usb_defs.h) */
#define MSC_SUBCLASS_SCSI 0x06 /* US_SC_SCSI: SCSI transparent command set */
#define MSC_PROTO_BULK 0x50	   /* US_PR_BULK: Bulk-Only Transport */

/* Class-specific control requests (bRequest), recipient = interface */
#define MSC_REQ_RESET 0xFF	  /* Bulk-Only Mass Storage Reset */
#define MSC_REQ_GET_MAX_LUN 0xFE

/* Standard endpoint feature selector for Clear-Feature(ENDPOINT_HALT) */
#define USB_ENDPOINT_HALT 0

/* Command/Status Block Wrapper signatures ("USBC"/"USBS", little-endian) */
#define CBW_SIGNATURE 0x43425355u
#define CSW_SIGNATURE 0x53425355u
#define CBW_FLAG_DATA_IN 0x80

/* CSW status values */
#define CSW_STATUS_PASS 0x00
#define CSW_STATUS_FAIL 0x01
#define CSW_STATUS_PHASE_ERR 0x02

/* SCSI opcodes */
#define SCSI_TEST_UNIT_READY 0x00
#define SCSI_REQUEST_SENSE 0x03
#define SCSI_INQUIRY 0x12
#define SCSI_READ_CAPACITY_10 0x25
#define SCSI_READ_10 0x28

struct __attribute__((packed)) bot_cbw {
	uint32_t dCBWSignature;
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWCBLength;
	uint8_t CBWCB[16];
};

struct __attribute__((packed)) bot_csw {
	uint32_t dCSWSignature;
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t bCSWStatus;
};

static struct usb_device *msc_dev;
static uint8_t ep_in;  /* bulk IN endpoint address (0x8x) */
static uint8_t ep_out; /* bulk OUT endpoint address (0x0x) */
static uint8_t max_lun;
static uint32_t cbw_tag;
static uint32_t blk_size = 512; /* cached from READ CAPACITY */

/* DMA buffers. Each is 64-byte aligned AND occupies a whole cache line: the
 * xHCI driver invalidates the entire cache line(s) spanning a bulk buffer (see
 * xhci_inval_cache), so a shorter buffer sharing a line with another variable
 * could have that neighbour's cached write discarded on an IN transfer. Backing
 * the 31-byte CBW / 13-byte CSW with full-line byte arrays keeps them isolated.
 */
#define MSC_CACHELINE 64
static uint8_t cbw_raw[MSC_CACHELINE] __attribute__((aligned(MSC_CACHELINE)));
static uint8_t csw_raw[MSC_CACHELINE] __attribute__((aligned(MSC_CACHELINE)));
#define CBW ((struct bot_cbw *)cbw_raw)
#define CSW ((struct bot_csw *)csw_raw)

static int clear_ep_halt(uint8_t ep_addr)
{
	int ret = usb_control_msg(msc_dev, usb_sndctrlpipe(msc_dev, 0),
				  USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT,
				  USB_ENDPOINT_HALT, ep_addr, NULL, 0,
				  USB_CNTL_TIMEOUT);
	return ret < 0 ? ret : 0;
}

/*
 * Run one BOT command: send the CBW, transfer the data phase (if any), and
 * read the CSW. Returns the CSW status (0 = pass, 1 = fail/check-condition,
 * 2 = phase error) on a well-formed exchange, or negative on transport error.
 */
static int bot_command(const uint8_t *cdb, int cdb_len, bool data_in,
		       void *data, uint32_t data_len)
{
	if (!msc_dev || !ep_in || !ep_out)
		return -ENODEV;
	if (cdb_len < 1 || cdb_len > 16)
		return -EINVAL;

	/* --- Command phase: 31-byte CBW on bulk OUT --- */
	memset(cbw_raw, 0, sizeof(struct bot_cbw));
	CBW->dCBWSignature = CBW_SIGNATURE;
	CBW->dCBWTag = ++cbw_tag;
	CBW->dCBWDataTransferLength = data_len;
	CBW->bmCBWFlags = data_in ? CBW_FLAG_DATA_IN : 0;
	CBW->bCBWLUN = 0;
	CBW->bCBWCBLength = cdb_len;
	memcpy(CBW->CBWCB, cdb, cdb_len);

	unsigned long out_pipe =
		usb_sndbulkpipe(msc_dev, ep_out & USB_ENDPOINT_NUMBER_MASK);
	unsigned long in_pipe =
		usb_rcvbulkpipe(msc_dev, ep_in & USB_ENDPOINT_NUMBER_MASK);

	int ret = submit_bulk_msg(msc_dev, out_pipe, cbw_raw,
				  sizeof(struct bot_cbw));
	if (ret < 0) {
		printf("MSC: CBW send failed: %d\n", ret);
		return ret;
	}

	/* --- Data phase (optional) --- */
	if (data_len && data) {
		unsigned long pipe = data_in ? in_pipe : out_pipe;
		ret = submit_bulk_msg(msc_dev, pipe, data, data_len);
		if (ret < 0) {
			/* A stalled data phase is recoverable: clear the halt,
			 * then still read the CSW to complete the command. */
			clear_ep_halt(data_in ? ep_in : ep_out);
		}
	}

	/* --- Status phase: 13-byte CSW on bulk IN --- */
	memset(csw_raw, 0, sizeof(struct bot_csw));
	ret = submit_bulk_msg(msc_dev, in_pipe, csw_raw, sizeof(struct bot_csw));
	if (ret < 0) {
		/* CSW stalled: clear halt and retry once. */
		clear_ep_halt(ep_in);
		ret = submit_bulk_msg(msc_dev, in_pipe, csw_raw,
				      sizeof(struct bot_csw));
		if (ret < 0) {
			printf("MSC: CSW read failed: %d\n", ret);
			return ret;
		}
	}

	if (msc_dev->act_len < (int)sizeof(struct bot_csw) ||
	    CSW->dCSWSignature != CSW_SIGNATURE) {
		printf("MSC: bad CSW (sig=%08lx len=%d)\n",
		       (unsigned long)CSW->dCSWSignature, msc_dev->act_len);
		return -EIO;
	}
	if (CSW->dCSWTag != CBW->dCBWTag) {
		printf("MSC: CSW tag mismatch\n");
		return -EIO;
	}

	return CSW->bCSWStatus;
}

int usb_msc_init(struct usb_device *dev)
{
	msc_dev = NULL;
	ep_in = 0;
	ep_out = 0;
	max_lun = 0;
	cbw_tag = 0;

	int msc_ifnum = -1;

	for (int i = 0; i < dev->config.no_of_if; i++) {
		struct usb_interface *iface = &dev->config.if_desc[i];
		if (iface->desc.bInterfaceClass != USB_CLASS_MASS_STORAGE ||
		    iface->desc.bInterfaceSubClass != MSC_SUBCLASS_SCSI ||
		    iface->desc.bInterfaceProtocol != MSC_PROTO_BULK)
			continue;

		for (int e = 0; e < iface->no_of_ep; e++) {
			struct usb_endpoint_descriptor *ep = &iface->ep_desc[e];
			if ((ep->bmAttributes & 0x03) != USB_ENDPOINT_XFER_BULK)
				continue;
			if ((ep->bEndpointAddress & USB_DIR_IN) && !ep_in)
				ep_in = ep->bEndpointAddress;
			else if (!(ep->bEndpointAddress & USB_DIR_IN) && !ep_out)
				ep_out = ep->bEndpointAddress;
		}

		if (ep_in && ep_out) {
			msc_ifnum = iface->desc.bInterfaceNumber;
			break;
		}
		/* Interface matched but endpoints incomplete; keep looking. */
		ep_in = 0;
		ep_out = 0;
	}

	if (msc_ifnum < 0) {
		printf("USB MSC: no Bulk-Only SCSI interface found\n");
		return -ENODEV;
	}

	/* Get Max LUN (class request; a STALL means single-LUN, treat as 0). */
	uint8_t lun_buf[MSC_CACHELINE] __attribute__((aligned(MSC_CACHELINE))) = {0};
	int ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
				  MSC_REQ_GET_MAX_LUN,
				  USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				  0, msc_ifnum, lun_buf, 1, USB_CNTL_TIMEOUT);
	max_lun = (ret == 1) ? lun_buf[0] : 0;

	msc_dev = dev;
	printf("USB MSC: bound to interface %d\n", msc_ifnum);
	printf("  EP IN  0x%02x  EP OUT 0x%02x  max LUN %d\n", ep_in, ep_out,
	       max_lun);
	return 0;
}

void usb_msc_disconnect(void)
{
	msc_dev = NULL;
	ep_in = 0;
	ep_out = 0;
	max_lun = 0;
}

bool usb_msc_is_connected(void)
{
	return msc_dev != NULL;
}

/*
 * SCSI REQUEST SENSE. Reads and discards the sense data, which is what clears a
 * pending "unit attention" (e.g. power-on / media-change) so the next command
 * can succeed. Returns the sense key, or negative on transport error.
 */
static int request_sense(void)
{
	static uint8_t data[MSC_CACHELINE] __attribute__((aligned(MSC_CACHELINE)));
	enum { SENSE_LEN = 18 };
	uint8_t cdb[6] = {SCSI_REQUEST_SENSE, 0, 0, 0, SENSE_LEN, 0};

	memset(data, 0, sizeof(data));
	int st = bot_command(cdb, sizeof(cdb), true, data, SENSE_LEN);
	if (st < 0)
		return st;
	return data[2] & 0x0F; /* sense key */
}

int usb_msc_test_unit_ready(void)
{
	uint8_t cdb[6] = {SCSI_TEST_UNIT_READY, 0, 0, 0, 0, 0};
	int st = bot_command(cdb, sizeof(cdb), true, NULL, 0);
	if (st < 0)
		return st;
	if (st == CSW_STATUS_PASS)
		return 0;

	/* Not ready: drain the sense data (clears unit-attention) and report. */
	request_sense();
	return 1;
}

int usb_msc_inquiry(char *vendor, char *product, uint8_t *device_type)
{
	static uint8_t data[MSC_CACHELINE] __attribute__((aligned(MSC_CACHELINE)));
	enum { INQUIRY_LEN = 36 };
	uint8_t cdb[6] = {SCSI_INQUIRY, 0, 0, 0, INQUIRY_LEN, 0};

	int st = bot_command(cdb, sizeof(cdb), true, data, INQUIRY_LEN);
	if (st < 0)
		return st;
	if (st != CSW_STATUS_PASS)
		return -EIO;

	if (device_type)
		*device_type = data[0] & 0x1F;
	if (vendor) {
		memcpy(vendor, &data[8], 8);
		vendor[8] = '\0';
	}
	if (product) {
		memcpy(product, &data[16], 16);
		product[16] = '\0';
	}
	return 0;
}

int usb_msc_read_capacity(uint32_t *last_lba, uint32_t *block_size)
{
	static uint8_t data[MSC_CACHELINE] __attribute__((aligned(MSC_CACHELINE)));
	enum { CAPACITY_LEN = 8 };
	uint8_t cdb[10] = {SCSI_READ_CAPACITY_10, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	int st = bot_command(cdb, sizeof(cdb), true, data, CAPACITY_LEN);
	if (st < 0)
		return st;
	if (st != CSW_STATUS_PASS)
		return -EIO;

	/* Response fields are big-endian. */
	uint32_t lba = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
		       ((uint32_t)data[2] << 8) | data[3];
	uint32_t bs = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
		      ((uint32_t)data[6] << 8) | data[7];
	if (bs)
		blk_size = bs; /* cache for usb_msc_read_blocks() */
	if (last_lba)
		*last_lba = lba;
	if (block_size)
		*block_size = bs;
	return 0;
}

int usb_msc_read_blocks(uint32_t lba, uint32_t count, void *buf)
{
	uint8_t cdb[10];
	cdb[0] = SCSI_READ_10;
	cdb[1] = 0;
	cdb[2] = (lba >> 24) & 0xFF; /* LBA big-endian */
	cdb[3] = (lba >> 16) & 0xFF;
	cdb[4] = (lba >> 8) & 0xFF;
	cdb[5] = lba & 0xFF;
	cdb[6] = 0;
	cdb[7] = (count >> 8) & 0xFF; /* transfer length in blocks, big-endian */
	cdb[8] = count & 0xFF;
	cdb[9] = 0;

	int st = bot_command(cdb, sizeof(cdb), true, buf, count * blk_size);
	if (st < 0)
		return st;
	return st == CSW_STATUS_PASS ? 0 : -EIO;
}
