/*
 * xhci_baremetal.c — Bare-metal xHCI host mode for DWC3
 *
 * Provides:
 *   - xhci_host_init()   : initialize xHCI after dwc3_baremetal_init(HOST)
 *   - xhci_host_poll()   : poll for port changes and enumerate new devices
 *   - usb_maxpacket()    : USB core shim needed by xhci-ring.c
 *   - USB device table   : non-DM device tracking
 */

#include <common.h>
#include <usb.h>
#include <usb/xhci.h>
#include <linux/delay.h>
#include <string.h>

/* ---- USB device table (non-DM) ---- */

static struct usb_device usb_dev_table[USB_MAX_DEVICE];
static int usb_dev_count;

struct usb_device *usb_get_dev_index(int index)
{
	if (index < 0 || index >= usb_dev_count)
		return NULL;
	return &usb_dev_table[index];
}

int usb_alloc_new_device(void *controller, struct usb_device **devp)
{
	if (usb_dev_count >= USB_MAX_DEVICE) {
		printf("usb_alloc_new_device: too many devices\n");
		return -1;
	}
	struct usb_device *dev = &usb_dev_table[usb_dev_count];
	memset(dev, 0, sizeof(*dev));
	dev->devnum = usb_dev_count + 1; /* USB devnum starts at 1 */
	dev->controller = controller;
	dev->maxpacketsize = PACKET_SIZE_64;
	dev->speed = USB_SPEED_UNKNOWN;
	*devp = dev;
	usb_dev_count++;
	return 0;
}

void usb_free_device(void *controller)
{
	if (usb_dev_count > 0)
		usb_dev_count--;
}

int usb_maxpacket(struct usb_device *dev, unsigned long pipe)
{
	if (usb_pipein(pipe))
		return dev->epmaxpacketin[usb_pipeendpoint(pipe)];
	else
		return dev->epmaxpacketout[usb_pipeendpoint(pipe)];
}

/* ---- Stub functions declared in usb.h ---- */

int usb_new_device(struct usb_device *dev)
{
	return 0;
}

int usb_update_hub_device(struct usb_device *dev)
{
	return 0;
}

bool usb_device_has_child_on_port(struct usb_device *parent, int port)
{
	return false;
}

int usb_hub_probe(struct usb_device *dev, int ifnum)
{
	return 0;
}

void usb_hub_reset(void)
{
}

void usb_find_usb2_hub_address_port(struct usb_device *udev,
				    uint8_t *hub_address, uint8_t *hub_port)
{
	struct usb_device *hop = udev;
	if (hop->parent) {
		while (hop->parent->parent)
			hop = hop->parent;
		*hub_port = hop->portnr;
		*hub_address = hop->parent->devnum;
	} else {
		*hub_port = 0;
		*hub_address = 0;
	}
}

int usb_control_msg(struct usb_device *dev, unsigned int pipe,
		    unsigned char request, unsigned char requesttype,
		    unsigned short value, unsigned short index,
		    void *data, unsigned short size, int timeout)
{
	struct devrequest setup;
	setup.requesttype = requesttype;
	setup.request = request;
	setup.value = cpu_to_le16(value);
	setup.index = cpu_to_le16(index);
	setup.length = cpu_to_le16(size);

	int ret = submit_control_msg(dev, usb_sndctrlpipe(dev, 0),
				     data, size, &setup);
	if (ret < 0)
		return ret;
	return dev->act_len;
}

int usb_set_interface(struct usb_device *dev, int interface, int alternate)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
			       USB_REQ_SET_INTERFACE, USB_RECIP_INTERFACE,
			       alternate, interface, NULL, 0, USB_CNTL_TIMEOUT);
}

int usb_get_port_status(struct usb_device *dev, int port, void *data)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			       USB_REQ_GET_STATUS, USB_DIR_IN | USB_RT_PORT,
			       0, port, data, sizeof(struct usb_port_status),
			       USB_CNTL_TIMEOUT);
}

/* ---- xHCI host init/shutdown/poll ---- */

static struct xhci_ctrl xhci_ctrl_instance;
static struct usb_device *root_hub;
static bool device_connected;

static const char *speed_str(enum usb_device_speed speed)
{
	switch (speed) {
	case USB_SPEED_LOW:	return "Low (1.5 Mbps)";
	case USB_SPEED_FULL:	return "Full (12 Mbps)";
	case USB_SPEED_HIGH:	return "High (480 Mbps)";
	case USB_SPEED_SUPER:	return "Super (5 Gbps)";
	default:		return "Unknown";
	}
}

static enum usb_device_speed portsc_to_speed(u32 portsc)
{
	switch (portsc & DEV_SPEED_MASK) {
	case XDEV_FS: return USB_SPEED_FULL;
	case XDEV_LS: return USB_SPEED_LOW;
	case XDEV_HS: return USB_SPEED_HIGH;
	case XDEV_SS: return USB_SPEED_SUPER;
	default:      return USB_SPEED_UNKNOWN;
	}
}

/*
 * Get a USB string descriptor and decode it to ASCII.
 */
static int usb_get_string(struct usb_device *dev, int index,
			  char *buf, int buflen)
{
	unsigned char tbuf[256];
	int ret;

	if (index == 0 || buflen < 2)
		return 0;

	/* First get the language ID list (string 0) to get wLANGID[0] */
	ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			      USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			      (USB_DT_STRING << 8) | 0, 0,
			      tbuf, 4, USB_CNTL_TIMEOUT);
	if (ret < 4)
		return 0;

	u16 langid = tbuf[2] | (tbuf[3] << 8);

	/* Now get the actual string */
	ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			      USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			      (USB_DT_STRING << 8) | index, langid,
			      tbuf, sizeof(tbuf), USB_CNTL_TIMEOUT);
	if (ret < 2)
		return 0;

	/* Decode UTF-16LE to ASCII */
	int slen = (tbuf[0] - 2) / 2;
	if (slen > buflen - 1)
		slen = buflen - 1;
	for (int i = 0; i < slen; i++)
		buf[i] = tbuf[2 + i * 2]; /* low byte of UTF-16 */
	buf[slen] = '\0';
	return slen;
}

/*
 * Enumerate a newly connected device:
 *   1. Allocate device slot (Enable Slot command)
 *   2. Set Address (Address Device command)
 *   3. Read device descriptor
 *   4. Read config descriptor
 *   5. Set Configuration
 */
static int xhci_enumerate_device(int port, u32 portsc)
{
	struct xhci_ctrl *ctrl = &xhci_ctrl_instance;
	struct usb_device *dev;
	int ret;

	/* Allocate a usb_device for this new device */
	ret = usb_alloc_new_device(ctrl, &dev);
	if (ret) {
		printf("  Failed to allocate USB device\n");
		return ret;
	}

	dev->speed = portsc_to_speed(portsc);
	dev->portnr = port;
	dev->parent = root_hub;
	dev->epmaxpacketin[0] = 64;
	dev->epmaxpacketout[0] = 64;

	printf("  Speed: %s\n", speed_str(dev->speed));

	/* Enable Slot — xHCI allocates a slot and virtual device structs */
	ret = usb_alloc_device(dev);
	if (ret) {
		printf("  Enable Slot failed: %d\n", ret);
		usb_free_device(ctrl);
		return ret;
	}
	printf("  Slot %d assigned\n", dev->slot_id);

	/* Set Address (USB_REQ_SET_ADDRESS) — routed to xhci_address_device */
	ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
			      USB_REQ_SET_ADDRESS, 0,
			      dev->devnum, 0, NULL, 0, USB_CNTL_TIMEOUT);
	if (ret < 0) {
		printf("  Set Address failed: %d\n", ret);
		return ret;
	}
	printf("  Address set\n");

	/* Get Device Descriptor (first 8 bytes to learn bMaxPacketSize0) */
	struct usb_device_descriptor *desc = &dev->descriptor;
	ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			      USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			      (USB_DT_DEVICE << 8), 0,
			      desc, 8, USB_CNTL_TIMEOUT);
	if (ret < 8) {
		printf("  Get Descriptor (8) failed: %d\n", ret);
		return ret;
	}

	/* Update ep0 max packet size */
	dev->epmaxpacketin[0] = desc->bMaxPacketSize0;
	dev->epmaxpacketout[0] = desc->bMaxPacketSize0;

	/* Get full device descriptor */
	ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			      USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			      (USB_DT_DEVICE << 8), 0,
			      desc, sizeof(*desc), USB_CNTL_TIMEOUT);
	if (ret < (int)sizeof(*desc)) {
		printf("  Get Descriptor (full) failed: %d\n", ret);
		return ret;
	}

	/* Read manufacturer, product, serial strings */
	usb_get_string(dev, desc->iManufacturer, dev->mf, sizeof(dev->mf));
	usb_get_string(dev, desc->iProduct, dev->prod, sizeof(dev->prod));
	usb_get_string(dev, desc->iSerialNumber, dev->serial, sizeof(dev->serial));

	printf("  Device: %04x:%04x", le16_to_cpu(desc->idVendor),
	       le16_to_cpu(desc->idProduct));
	if (dev->mf[0])
		printf(" %s", dev->mf);
	if (dev->prod[0])
		printf(" %s", dev->prod);
	printf("\n");
	printf("  Class: %02x/%02x/%02x, %d config(s)\n",
	       desc->bDeviceClass, desc->bDeviceSubClass,
	       desc->bDeviceProtocol, desc->bNumConfigurations);

	if (desc->bNumConfigurations == 0) {
		printf("  No configurations!\n");
		return -ENODEV;
	}

	/* Get Config Descriptor (first 9 bytes to learn wTotalLength) */
	struct usb_config_descriptor cfg_hdr;
	ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			      USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			      (USB_DT_CONFIG << 8), 0,
			      &cfg_hdr, sizeof(cfg_hdr), USB_CNTL_TIMEOUT);
	if (ret < (int)sizeof(cfg_hdr)) {
		printf("  Get Config Descriptor failed: %d\n", ret);
		return ret;
	}

	u16 total_len = le16_to_cpu(cfg_hdr.wTotalLength);
	printf("  Config %d: %d interface(s), total %d bytes\n",
	       cfg_hdr.bConfigurationValue, cfg_hdr.bNumInterfaces, total_len);

	/* Get full config descriptor */
	unsigned char cfg_buf[512];
	if (total_len > sizeof(cfg_buf))
		total_len = sizeof(cfg_buf);
	ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			      USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			      (USB_DT_CONFIG << 8), 0,
			      cfg_buf, total_len, USB_CNTL_TIMEOUT);
	if (ret < (int)total_len) {
		printf("  Get Config (full) failed: %d (got %d)\n", ret, ret);
		/* Non-fatal, continue with what we got */
	}

	/* Parse interface descriptors for display */
	int pos = cfg_hdr.bLength;
	while (pos + 2 <= total_len) {
		u8 dlen = cfg_buf[pos];
		u8 dtype = cfg_buf[pos + 1];
		if (dlen < 2)
			break;
		if (dtype == USB_DT_INTERFACE && dlen >= 9) {
			struct usb_interface_descriptor *intf =
				(struct usb_interface_descriptor *)&cfg_buf[pos];
			printf("    Interface %d: class %02x/%02x/%02x, %d endpoints\n",
			       intf->bInterfaceNumber,
			       intf->bInterfaceClass,
			       intf->bInterfaceSubClass,
			       intf->bInterfaceProtocol,
			       intf->bNumEndpoints);
		}
		pos += dlen;
	}

	/* Set Configuration */
	ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
			      USB_REQ_SET_CONFIGURATION, 0,
			      cfg_hdr.bConfigurationValue, 0,
			      NULL, 0, USB_CNTL_TIMEOUT);
	if (ret < 0) {
		printf("  Set Configuration failed: %d\n", ret);
		return ret;
	}
	printf("  Configured (config %d)\n", cfg_hdr.bConfigurationValue);

	dev->configno = cfg_hdr.bConfigurationValue;
	device_connected = true;
	return 0;
}

/*
 * xhci_host_poll — check port status and handle connect/disconnect.
 *
 * Call this periodically from your main loop.
 * Returns the newly enumerated usb_device on connect, NULL otherwise.
 */
struct usb_device *xhci_host_poll(void)
{
	struct xhci_ctrl *ctrl = &xhci_ctrl_instance;
	struct xhci_hcor *hcor = ctrl->hcor;
	volatile uint32_t *portsc_reg = &hcor->portregs[0].or_portsc;
	u32 portsc = xhci_readl(portsc_reg);

	/* Check for port status change bits */
	if (!(portsc & (PORT_CSC | PORT_PEC | PORT_RC)))
		return NULL;

	printf("Port status change: PORTSC=0x%08x", portsc);
	if (portsc & PORT_CSC)
		printf(" [connect]");
	if (portsc & PORT_PEC)
		printf(" [enable]");
	if (portsc & PORT_RC)
		printf(" [reset]");
	printf("\n");

	/* Clear change bits (write-1-to-clear) while preserving RWS bits */
	u32 clear = portsc;
	/* Preserve only RO and RWS bits, set change bits to clear them */
	clear &= XHCI_PORT_RO | XHCI_PORT_RWS;
	clear |= portsc & (PORT_CSC | PORT_PEC | PORT_RC | PORT_WRC |
			   PORT_OCC | PORT_PLC | PORT_CEC);
	xhci_writel(portsc_reg, clear);

	/* Connection Status Change */
	if (portsc & PORT_CSC) {
		if (portsc & PORT_CONNECT) {
			printf("Device connected\n");

			/* Issue port reset */
			u32 val = xhci_readl(portsc_reg);
			val &= XHCI_PORT_RO | XHCI_PORT_RWS;
			val |= PORT_RESET;
			xhci_writel(portsc_reg, val);

			/* Wait for reset to complete */
			int timeout = 500;
			while (timeout-- > 0) {
				mdelay(1);
				portsc = xhci_readl(portsc_reg);
				if (portsc & PORT_RC)
					break;
			}
			if (!(portsc & PORT_RC)) {
				printf("Port reset timeout!\n");
				return NULL;
			}

			/* Clear RC bit */
			val = portsc;
			val &= XHCI_PORT_RO | XHCI_PORT_RWS;
			val |= PORT_RC;
			xhci_writel(portsc_reg, val);

			portsc = xhci_readl(portsc_reg);
			printf("Post-reset PORTSC=0x%08x\n", portsc);

			if (!(portsc & PORT_PE)) {
				printf("Port not enabled after reset\n");
				return NULL;
			}

			/* Enumerate */
			int ret = xhci_enumerate_device(1, portsc);
			if (ret)
				printf("Enumeration failed: %d\n", ret);
			else
				return usb_get_dev_index(usb_dev_count - 1);

		} else {
			printf("Device disconnected\n");
			device_connected = false;
			/* TODO: disable slot, cleanup */
		}
	}

	return NULL;
}

int xhci_host_init(uintptr_t dwc3_base)
{
	struct xhci_hccr *hccr;
	struct xhci_hcor *hcor;

	memset(&xhci_ctrl_instance, 0, sizeof(xhci_ctrl_instance));
	usb_dev_count = 0;
	device_connected = false;

	hccr = (struct xhci_hccr *)dwc3_base;
	hcor = (struct xhci_hcor *)(dwc3_base +
		HC_LENGTH(xhci_readl(&hccr->cr_capbase)));

	printf("xHCI: cap_length=%u, hci_version=0x%04x\n",
	       HC_LENGTH(xhci_readl(&hccr->cr_capbase)),
	       HC_VERSION(xhci_readl(&hccr->cr_capbase)));

	int ret = xhci_register_bare(&xhci_ctrl_instance, hccr, hcor);
	if (ret) {
		printf("xhci_register_bare failed: %d\n", ret);
		return ret;
	}

	int max_ports = HCS_MAX_PORTS(xhci_readl(&hccr->cr_hcsparams1));
	printf("xHCI: initialized, %d port(s)\n", max_ports);

	/* Allocate root hub as device 0 */
	ret = usb_alloc_new_device(&xhci_ctrl_instance, &root_hub);
	if (ret)
		return ret;
	root_hub->speed = USB_SPEED_HIGH;

	/* Tell xHCI this is the root hub (Enable Slot is skipped for rootdev==0) */
	ret = usb_alloc_device(root_hub);
	if (ret) {
		printf("Root hub alloc failed: %d\n", ret);
		return ret;
	}

	/* Set rootdev directly so control transfers to real devices
	 * don't get routed to the root hub handler. */
	xhci_ctrl_instance.rootdev = root_hub->devnum;

	/* Power on port(s) */
	for (int i = 0; i < max_ports; i++) {
		volatile uint32_t *ps = &hcor->portregs[i].or_portsc;
		u32 val = xhci_readl(ps);
		if (!(val & PORT_POWER)) {
			val &= XHCI_PORT_RO | XHCI_PORT_RWS;
			val |= PORT_POWER;
			xhci_writel(ps, val);
			mdelay(20);
		}
		printf("  Port %d: PORTSC=0x%08x\n", i + 1, xhci_readl(ps));
	}

	return 0;
}

struct xhci_ctrl *xhci_host_get_ctrl(void)
{
	return &xhci_ctrl_instance;
}

void xhci_host_shutdown(void)
{
	xhci_deregister_bare(&xhci_ctrl_instance);
}
