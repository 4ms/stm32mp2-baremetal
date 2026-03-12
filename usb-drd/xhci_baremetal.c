/*
 * xhci_baremetal.c — Bare-metal xHCI host mode for DWC3
 *
 * Provides:
 *   - xhci_host_init()   : initialize xHCI after dwc3_baremetal_init(HOST)
 *   - xhci_host_poll()   : poll for port changes
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
	dev->devnum = usb_dev_count;
	dev->controller = controller;
	dev->maxpacketsize = PACKET_SIZE_64;
	dev->speed = USB_SPEED_UNKNOWN;
	*devp = dev;
	usb_dev_count++;
	return 0;
}

void usb_free_device(void *controller)
{
	/* Simple: just decrement count (last allocated) */
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

/* ---- Stub functions declared in usb.h but not needed yet ---- */

int usb_new_device(struct usb_device *dev)
{
	/* Will be implemented when we add enumeration */
	debug("usb_new_device: devnum=%d\n", dev->devnum);
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
	/* Walk up to the root hub */
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

/* ---- xHCI host init/shutdown ---- */

static struct xhci_ctrl xhci_ctrl_instance;

int xhci_host_init(uintptr_t dwc3_base)
{
	struct xhci_hccr *hccr;
	struct xhci_hcor *hcor;

	memset(&xhci_ctrl_instance, 0, sizeof(xhci_ctrl_instance));
	usb_dev_count = 0;

	/* xHCI capability registers are at the DWC3 base */
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

	printf("xHCI: initialized, %d ports\n",
	       HCS_MAX_PORTS(xhci_readl(&hccr->cr_hcsparams1)));

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
