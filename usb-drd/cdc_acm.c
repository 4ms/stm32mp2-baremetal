/*
 * cdc_acm.c — Minimal CDC-ACM (USB serial) gadget driver
 *
 * Sits directly on the DWC3 gadget API, no composite framework.
 * Enumerates as a virtual serial port on the host.
 */

#include "cdc_acm.h"
#include <common.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/dma-mapping.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Configuration
 * ---------------------------------------------------------------- */

#ifndef CDC_ACM_VENDOR_ID
#define CDC_ACM_VENDOR_ID	0x1209	/* pid.codes open-source VID */
#endif
#ifndef CDC_ACM_PRODUCT_ID
#define CDC_ACM_PRODUCT_ID	0x0001
#endif

#define CDC_ACM_BULK_EP_OUT	"ep1out"
#define CDC_ACM_BULK_EP_IN	"ep1in"
#define CDC_ACM_NOTIFY_EP_IN	"ep2in"

#define EP_ADDR_BULK_OUT	0x01
#define EP_ADDR_BULK_IN		0x81
#define EP_ADDR_NOTIFY_IN	0x82

#define BULK_MAX_PACKET		512
#define NOTIFY_MAX_PACKET	8
#define EP0_BUF_SIZE		256
#define DATA_BUF_SIZE		512

/* ----------------------------------------------------------------
 * CDC class constants and descriptors
 * ---------------------------------------------------------------- */

/* CDC class/subclass */
#define USB_CLASS_COMM		0x02
#define USB_CLASS_CDC_DATA	0x0A
#define CDC_SUBCLASS_ACM	0x02
#define CDC_PROTOCOL_AT		0x01

/* CDC descriptor types */
#define USB_DT_CS_INTERFACE	0x24

/* CDC descriptor subtypes */
#define CDC_HEADER_TYPE		0x00
#define CDC_CALL_MGMT_TYPE	0x01
#define CDC_ACM_TYPE		0x02
#define CDC_UNION_TYPE		0x06

/* CDC class requests */
#define CDC_SET_LINE_CODING	0x20
#define CDC_GET_LINE_CODING	0x21
#define CDC_SET_CTRL_LINE_STATE	0x22

struct cdc_line_coding {
	u32 dwDTERate;
	u8  bCharFormat;
	u8  bParityType;
	u8  bDataBits;
} __packed;

/* CDC functional descriptors */
struct usb_cdc_header_desc {
	u8  bLength;
	u8  bDescriptorType;
	u8  bDescriptorSubtype;
	u16 bcdCDC;
} __packed;

struct usb_cdc_acm_desc {
	u8 bLength;
	u8 bDescriptorType;
	u8 bDescriptorSubtype;
	u8 bmCapabilities;
} __packed;

struct usb_cdc_union_desc {
	u8 bLength;
	u8 bDescriptorType;
	u8 bDescriptorSubtype;
	u8 bControlInterface;
	u8 bSubordinateInterface;
} __packed;

struct usb_cdc_call_mgmt_desc {
	u8 bLength;
	u8 bDescriptorType;
	u8 bDescriptorSubtype;
	u8 bmCapabilities;
	u8 bDataInterface;
} __packed;

/* Full configuration descriptor (packed contiguously) */
struct cdc_config_desc {
	struct usb_config_descriptor	config;
	/* Interface 0: CDC Communication */
	struct usb_interface_descriptor	comm_intf;
	struct usb_cdc_header_desc	cdc_header;
	struct usb_cdc_call_mgmt_desc	cdc_call_mgmt;
	struct usb_cdc_acm_desc		cdc_acm;
	struct usb_cdc_union_desc	cdc_union;
	struct usb_endpoint_descriptor	notify_ep;
	/* Interface 1: CDC Data */
	struct usb_interface_descriptor	data_intf;
	struct usb_endpoint_descriptor	out_ep;
	struct usb_endpoint_descriptor	in_ep;
} __packed;

/* ----------------------------------------------------------------
 * Descriptor data
 * ---------------------------------------------------------------- */

static const struct usb_device_descriptor device_desc = {
	.bLength		= USB_DT_DEVICE_SIZE,
	.bDescriptorType	= USB_DT_DEVICE,
	.bcdUSB			= cpu_to_le16(0x0200),
	.bDeviceClass		= USB_CLASS_COMM,
	.bDeviceSubClass	= 0,
	.bDeviceProtocol	= 0,
	.bMaxPacketSize0	= 64,
	.idVendor		= cpu_to_le16(CDC_ACM_VENDOR_ID),
	.idProduct		= cpu_to_le16(CDC_ACM_PRODUCT_ID),
	.bcdDevice		= cpu_to_le16(0x0100),
	.iManufacturer		= 1,
	.iProduct		= 2,
	.iSerialNumber		= 3,
	.bNumConfigurations	= 1,
};

static const struct cdc_config_desc config_desc = {
	.config = {
		.bLength		= USB_DT_CONFIG_SIZE,
		.bDescriptorType	= USB_DT_CONFIG,
		.wTotalLength		= cpu_to_le16(sizeof(struct cdc_config_desc)),
		.bNumInterfaces		= 2,
		.bConfigurationValue	= 1,
		.iConfiguration		= 0,
		.bmAttributes		= 0x80,
		.bMaxPower		= 250,	/* 500 mA */
	},
	.comm_intf = {
		.bLength		= USB_DT_INTERFACE_SIZE,
		.bDescriptorType	= USB_DT_INTERFACE,
		.bInterfaceNumber	= 0,
		.bNumEndpoints		= 1,
		.bInterfaceClass	= USB_CLASS_COMM,
		.bInterfaceSubClass	= CDC_SUBCLASS_ACM,
		.bInterfaceProtocol	= CDC_PROTOCOL_AT,
	},
	.cdc_header = {
		.bLength		= sizeof(struct usb_cdc_header_desc),
		.bDescriptorType	= USB_DT_CS_INTERFACE,
		.bDescriptorSubtype	= CDC_HEADER_TYPE,
		.bcdCDC			= cpu_to_le16(0x0110),
	},
	.cdc_call_mgmt = {
		.bLength		= sizeof(struct usb_cdc_call_mgmt_desc),
		.bDescriptorType	= USB_DT_CS_INTERFACE,
		.bDescriptorSubtype	= CDC_CALL_MGMT_TYPE,
		.bmCapabilities		= 0x00,
		.bDataInterface		= 1,
	},
	.cdc_acm = {
		.bLength		= sizeof(struct usb_cdc_acm_desc),
		.bDescriptorType	= USB_DT_CS_INTERFACE,
		.bDescriptorSubtype	= CDC_ACM_TYPE,
		.bmCapabilities		= 0x02,	/* line coding + serial state */
	},
	.cdc_union = {
		.bLength		= sizeof(struct usb_cdc_union_desc),
		.bDescriptorType	= USB_DT_CS_INTERFACE,
		.bDescriptorSubtype	= CDC_UNION_TYPE,
		.bControlInterface	= 0,
		.bSubordinateInterface	= 1,
	},
	.notify_ep = {
		.bLength		= USB_DT_ENDPOINT_SIZE,
		.bDescriptorType	= USB_DT_ENDPOINT,
		.bEndpointAddress	= EP_ADDR_NOTIFY_IN,
		.bmAttributes		= USB_ENDPOINT_XFER_INT,
		.wMaxPacketSize		= cpu_to_le16(NOTIFY_MAX_PACKET),
		.bInterval		= 32,
	},
	.data_intf = {
		.bLength		= USB_DT_INTERFACE_SIZE,
		.bDescriptorType	= USB_DT_INTERFACE,
		.bInterfaceNumber	= 1,
		.bNumEndpoints		= 2,
		.bInterfaceClass	= USB_CLASS_CDC_DATA,
	},
	.out_ep = {
		.bLength		= USB_DT_ENDPOINT_SIZE,
		.bDescriptorType	= USB_DT_ENDPOINT,
		.bEndpointAddress	= EP_ADDR_BULK_OUT,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
		.wMaxPacketSize		= cpu_to_le16(BULK_MAX_PACKET),
	},
	.in_ep = {
		.bLength		= USB_DT_ENDPOINT_SIZE,
		.bDescriptorType	= USB_DT_ENDPOINT,
		.bEndpointAddress	= EP_ADDR_BULK_IN,
		.bmAttributes		= USB_ENDPOINT_XFER_BULK,
		.wMaxPacketSize		= cpu_to_le16(BULK_MAX_PACKET),
	},
};

static const char *string_table[] = {
	[1] = "Bare-Metal",
	[2] = "CDC-ACM Serial",
	[3] = "000000000001",
};

/* ----------------------------------------------------------------
 * Driver state
 * ---------------------------------------------------------------- */

static struct {
	struct usb_gadget	*gadget;

	/* Endpoints */
	struct usb_ep		*ep_in;
	struct usb_ep		*ep_out;
	struct usb_ep		*ep_notify;

	/* EP0 control response */
	struct usb_request	*ep0_req;

	/* Bulk data requests */
	struct usb_request	*rx_req;
	struct usb_request	*tx_req;

	/* Receive ring buffer */
	u8			rx_buf[DATA_BUF_SIZE];
	int			rx_head;
	int			rx_tail;

	/* TX state */
	bool			tx_busy;

	/* Device state */
	bool			configured;
	struct cdc_line_coding	line_coding;
	u16			ctrl_line_state;
} acm;

/* ----------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------- */

static struct usb_ep *find_ep(struct usb_gadget *gadget, const char *name)
{
	struct usb_ep *ep;
	list_for_each_entry(ep, &gadget->ep_list, ep_list) {
		if (strcmp(ep->name, name) == 0)
			return ep;
	}
	return NULL;
}

static int make_string_desc(const char *str, void *buf, int buflen)
{
	u8 *d = buf;
	int slen = strlen(str);
	int len = 2 + 2 * slen;

	if (len > buflen)
		len = buflen;
	d[0] = len;
	d[1] = USB_DT_STRING;
	for (int i = 0; i < (len - 2) / 2; i++) {
		d[2 + i * 2] = str[i];
		d[2 + i * 2 + 1] = 0;
	}
	return len;
}

/* Queue a pre-built response on EP0 */
static int ep0_reply(const void *data, int len, int wLength)
{
	if (len > wLength)
		len = wLength;
	memcpy(acm.ep0_req->buf, data, len);
	acm.ep0_req->length = len;
	acm.ep0_req->zero = (len < wLength);
	return usb_ep_queue(acm.gadget->ep0, acm.ep0_req, 0);
}

static void rx_push(const u8 *data, int len)
{
	for (int i = 0; i < len; i++) {
		int next = (acm.rx_head + 1) % DATA_BUF_SIZE;
		if (next == acm.rx_tail)
			break;	/* full */
		acm.rx_buf[acm.rx_head] = data[i];
		acm.rx_head = next;
	}
}

/* ----------------------------------------------------------------
 * Endpoint completion callbacks
 * ---------------------------------------------------------------- */

static void rx_complete(struct usb_ep *ep, struct usb_request *req);

static void rx_submit(void)
{
	acm.rx_req->length = DATA_BUF_SIZE;
	usb_ep_queue(acm.ep_out, acm.rx_req, 0);
}

static void rx_complete(struct usb_ep *ep, struct usb_request *req)
{
	if (req->status == 0 && req->actual > 0)
		rx_push(req->buf, req->actual);

	/* Re-queue for next packet */
	if (acm.configured)
		rx_submit();
}

static void tx_complete(struct usb_ep *ep, struct usb_request *req)
{
	acm.tx_busy = false;
}

static void ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
	/* Nothing to do — EP0 response delivered */
}

/* ----------------------------------------------------------------
 * Setup request handler
 * ---------------------------------------------------------------- */

static void enable_data_eps(void)
{
	acm.ep_out->ops->enable(acm.ep_out, &config_desc.out_ep);
	acm.ep_in->ops->enable(acm.ep_in, &config_desc.in_ep);
	acm.ep_notify->ops->enable(acm.ep_notify, &config_desc.notify_ep);
	acm.configured = true;
	acm.tx_busy = false;
	rx_submit();
}

static void disable_data_eps(void)
{
	acm.configured = false;
	acm.ep_out->ops->disable(acm.ep_out);
	acm.ep_in->ops->disable(acm.ep_in);
	acm.ep_notify->ops->disable(acm.ep_notify);
}

static int handle_get_descriptor(const struct usb_ctrlrequest *ctrl)
{
	u16 wValue = le16_to_cpu(ctrl->wValue);
	u16 wLength = le16_to_cpu(ctrl->wLength);
	u8 type = wValue >> 8;
	u8 index = wValue & 0xff;

	switch (type) {
	case USB_DT_DEVICE:
		return ep0_reply(&device_desc, sizeof(device_desc), wLength);

	case USB_DT_CONFIG:
		return ep0_reply(&config_desc, sizeof(config_desc), wLength);

	case USB_DT_STRING:
		if (index == 0) {
			/* Language ID */
			u8 lang[] = { 4, USB_DT_STRING, 0x09, 0x04 };
			return ep0_reply(lang, sizeof(lang), wLength);
		}
		if (index < ARRAY_SIZE(string_table) && string_table[index]) {
			int len = make_string_desc(string_table[index],
						   acm.ep0_req->buf,
						   EP0_BUF_SIZE);
			acm.ep0_req->length = min_t(int, len, wLength);
			acm.ep0_req->zero = (len < wLength);
			return usb_ep_queue(acm.gadget->ep0, acm.ep0_req, 0);
		}
		return -1;

	case USB_DT_DEVICE_QUALIFIER:
		/* USB 2.0 only — STALL to indicate no other-speed config */
		return -1;
	}
	return -1;
}

static void set_line_coding_complete(struct usb_ep *ep, struct usb_request *req)
{
	if (req->status == 0 && req->actual >= sizeof(struct cdc_line_coding))
		memcpy(&acm.line_coding, req->buf, sizeof(acm.line_coding));
}

static int handle_setup(struct usb_gadget *gadget,
			const struct usb_ctrlrequest *ctrl)
{
	u8 type = ctrl->bRequestType;
	u8 req = ctrl->bRequest;
	u16 wValue = le16_to_cpu(ctrl->wValue);
	u16 wLength = le16_to_cpu(ctrl->wLength);

	/* Standard requests */
	if ((type & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
		switch (req) {
		case USB_REQ_GET_DESCRIPTOR:
			return handle_get_descriptor(ctrl);

		case USB_REQ_SET_CONFIGURATION:
			if (wValue == 1) {
				enable_data_eps();
				return 0;
			} else if (wValue == 0) {
				disable_data_eps();
				return 0;
			}
			return -1;

		case USB_REQ_GET_CONFIGURATION: {
			u8 config = acm.configured ? 1 : 0;
			return ep0_reply(&config, 1, wLength);
		}
		}
		return -1;
	}

	/* CDC class requests */
	if ((type & USB_TYPE_MASK) == USB_TYPE_CLASS) {
		switch (req) {
		case CDC_SET_LINE_CODING:
			/* Host sends 7 bytes of line coding in data phase */
			acm.ep0_req->length = min_t(int, wLength,
						    sizeof(acm.line_coding));
			acm.ep0_req->complete = set_line_coding_complete;
			return usb_ep_queue(gadget->ep0, acm.ep0_req, 0);

		case CDC_GET_LINE_CODING:
			return ep0_reply(&acm.line_coding,
					 sizeof(acm.line_coding), wLength);

		case CDC_SET_CTRL_LINE_STATE:
			acm.ctrl_line_state = wValue;
			return 0;
		}
	}

	return -1;
}

/* ----------------------------------------------------------------
 * Gadget driver callbacks
 * ---------------------------------------------------------------- */

static int cdc_bind(struct usb_gadget *gadget, struct usb_gadget_driver *driver)
{
	unsigned long dma_handle;

	acm.gadget = gadget;

	/* Find endpoints */
	acm.ep_out = find_ep(gadget, CDC_ACM_BULK_EP_OUT);
	acm.ep_in = find_ep(gadget, CDC_ACM_BULK_EP_IN);
	acm.ep_notify = find_ep(gadget, CDC_ACM_NOTIFY_EP_IN);
	if (!acm.ep_out || !acm.ep_in || !acm.ep_notify) {
		pr_err("CDC-ACM: endpoints not found\n");
		return -ENODEV;
	}

	/* EP0 request for control responses */
	acm.ep0_req = usb_ep_alloc_request(gadget->ep0, 0);
	if (!acm.ep0_req)
		return -ENOMEM;
	acm.ep0_req->buf = dma_alloc_coherent(EP0_BUF_SIZE, &dma_handle);
	acm.ep0_req->complete = ep0_complete;

	/* Bulk OUT (RX) request */
	acm.rx_req = usb_ep_alloc_request(acm.ep_out, 0);
	if (!acm.rx_req)
		return -ENOMEM;
	acm.rx_req->buf = dma_alloc_coherent(DATA_BUF_SIZE, &dma_handle);
	acm.rx_req->complete = rx_complete;

	/* Bulk IN (TX) request */
	acm.tx_req = usb_ep_alloc_request(acm.ep_in, 0);
	if (!acm.tx_req)
		return -ENOMEM;
	acm.tx_req->buf = dma_alloc_coherent(DATA_BUF_SIZE, &dma_handle);
	acm.tx_req->complete = tx_complete;

	/* Default line coding: 115200 8N1 */
	acm.line_coding.dwDTERate = cpu_to_le32(115200);
	acm.line_coding.bCharFormat = 0;
	acm.line_coding.bParityType = 0;
	acm.line_coding.bDataBits = 8;

	return 0;
}

static void cdc_unbind(struct usb_gadget *gadget)
{
	/* Static allocator — nothing to free */
}

static void cdc_disconnect(struct usb_gadget *gadget)
{
	acm.configured = false;
	acm.tx_busy = false;
	acm.rx_head = acm.rx_tail = 0;
}

static void cdc_suspend(struct usb_gadget *gadget) {}
static void cdc_resume(struct usb_gadget *gadget) {}

static struct usb_gadget_driver cdc_driver = {
	.function	= "CDC-ACM",
	.max_speed	= USB_SPEED_HIGH,
	.bind		= cdc_bind,
	.unbind		= cdc_unbind,
	.setup		= handle_setup,
	.disconnect	= cdc_disconnect,
	.suspend	= cdc_suspend,
	.resume		= cdc_resume,
};

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */

int cdc_acm_init(struct usb_gadget *gadget)
{
	int ret;

	memset(&acm, 0, sizeof(acm));

	/* Register our gadget driver */
	ret = gadget->ops->udc_start(gadget, &cdc_driver);
	if (ret)
		return ret;

	/* Bind: allocate endpoints and requests */
	ret = cdc_driver.bind(gadget, &cdc_driver);
	if (ret)
		return ret;

	/* Connect to the bus */
	ret = gadget->ops->pullup(gadget, 1);
	if (ret)
		return ret;

	return 0;
}

bool cdc_acm_is_configured(void)
{
	return acm.configured;
}

int cdc_acm_write(const void *buf, int len)
{
	if (!acm.configured || acm.tx_busy)
		return 0;
	if (len > DATA_BUF_SIZE)
		len = DATA_BUF_SIZE;

	memcpy(acm.tx_req->buf, buf, len);
	acm.tx_req->length = len;
	acm.tx_req->zero = 0;
	acm.tx_busy = true;

	int ret = usb_ep_queue(acm.ep_in, acm.tx_req, 0);
	if (ret < 0) {
		acm.tx_busy = false;
		return ret;
	}
	return len;
}

int cdc_acm_read(void *buf, int max_len)
{
	u8 *dst = buf;
	int count = 0;

	while (count < max_len && acm.rx_tail != acm.rx_head) {
		dst[count++] = acm.rx_buf[acm.rx_tail];
		acm.rx_tail = (acm.rx_tail + 1) % DATA_BUF_SIZE;
	}
	return count;
}
