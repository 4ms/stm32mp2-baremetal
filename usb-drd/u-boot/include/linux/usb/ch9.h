#ifndef _LINUX_USB_CH9_H
#define _LINUX_USB_CH9_H

#include <linux/types.h>
#include <linux/compiler.h>

/* USB directions */
#define USB_DIR_OUT	0x00
#define USB_DIR_IN	0x80

/* USB request types */
#define USB_TYPE_MASK		(0x03 << 5)
#define USB_TYPE_STANDARD	(0x00 << 5)
#define USB_TYPE_CLASS		(0x01 << 5)
#define USB_TYPE_VENDOR		(0x02 << 5)
#define USB_TYPE_RESERVED	(0x03 << 5)

/* USB recipients */
#define USB_RECIP_MASK		0x1f
#define USB_RECIP_DEVICE	0x00
#define USB_RECIP_INTERFACE	0x01
#define USB_RECIP_ENDPOINT	0x02
#define USB_RECIP_OTHER		0x03

/* Standard requests */
#define USB_REQ_GET_STATUS		0x00
#define USB_REQ_CLEAR_FEATURE		0x01
#define USB_REQ_SET_FEATURE		0x03
#define USB_REQ_SET_ADDRESS		0x05
#define USB_REQ_GET_DESCRIPTOR		0x06
#define USB_REQ_SET_DESCRIPTOR		0x07
#define USB_REQ_GET_CONFIGURATION	0x08
#define USB_REQ_SET_CONFIGURATION	0x09
#define USB_REQ_GET_INTERFACE		0x0A
#define USB_REQ_SET_INTERFACE		0x0B
#define USB_REQ_SYNCH_FRAME		0x0C
#define USB_REQ_SET_SEL			0x30
#define USB_REQ_SET_ISOCH_DELAY		0x31

/* Feature selectors */
#define USB_DEVICE_SELF_POWERED		0
#define USB_DEVICE_REMOTE_WAKEUP	1
#define USB_DEVICE_TEST_MODE		2
#define USB_DEVICE_U1_ENABLE		48
#define USB_DEVICE_U2_ENABLE		49
#define USB_INTRF_FUNC_SUSPEND		0
#define USB_ENDPOINT_HALT		0

/* Test mode selectors */
#define USB_TEST_J		1
#define USB_TEST_K		2
#define USB_TEST_SE0_NAK	3
#define USB_TEST_PACKET		4
#define USB_TEST_FORCE_ENABLE	5

/* Descriptor types */
#define USB_DT_DEVICE			0x01
#define USB_DT_CONFIG			0x02
#define USB_DT_STRING			0x03
#define USB_DT_INTERFACE		0x04
#define USB_DT_ENDPOINT			0x05
#define USB_DT_DEVICE_QUALIFIER		0x06
#define USB_DT_OTHER_SPEED_CONFIG	0x07
#define USB_DT_BOS			0x0f
#define USB_DT_SS_ENDPOINT_COMP		0x30

/* Endpoint transfer types */
#define USB_ENDPOINT_XFERTYPE_MASK	0x03
#define USB_ENDPOINT_XFER_CONTROL	0
#define USB_ENDPOINT_XFER_ISOC		1
#define USB_ENDPOINT_XFER_BULK		2
#define USB_ENDPOINT_XFER_INT		3

/* Endpoint number mask */
#define USB_ENDPOINT_NUMBER_MASK	0x0f
#define USB_ENDPOINT_DIR_MASK		0x80

/* USB device speeds */
enum usb_device_speed {
	USB_SPEED_UNKNOWN = 0,
	USB_SPEED_LOW, USB_SPEED_FULL,
	USB_SPEED_HIGH, USB_SPEED_WIRELESS,
	USB_SPEED_SUPER,
};

/* USB device state */
enum usb_device_state {
	USB_STATE_NOTATTACHED = 0,
	USB_STATE_ATTACHED,
	USB_STATE_POWERED,
	USB_STATE_DEFAULT,
	USB_STATE_ADDRESS,
	USB_STATE_CONFIGURED,
	USB_STATE_SUSPENDED,
};

/* Control request (8-byte USB setup packet) */
struct usb_ctrlrequest {
	__u8  bRequestType;
	__u8  bRequest;
	__le16 wValue;
	__le16 wIndex;
	__le16 wLength;
} __packed;

/* Standard endpoint descriptor */
struct usb_endpoint_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bEndpointAddress;
	__u8  bmAttributes;
	__le16 wMaxPacketSize;
	__u8  bInterval;
} __packed;

/* SuperSpeed endpoint companion descriptor */
struct usb_ss_ep_comp_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bMaxBurst;
	__u8  bmAttributes;
	__le16 wBytesPerInterval;
} __packed;

/* Device qualifier descriptor */
struct usb_qualifier_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__le16 bcdUSB;
	__u8  bDeviceClass;
	__u8  bDeviceSubClass;
	__u8  bDeviceProtocol;
	__u8  bMaxPacketSize0;
	__u8  bNumConfigurations;
	__u8  bRESERVED;
} __packed;

/* BOS descriptor */
struct usb_bos_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__le16 wTotalLength;
	__u8  bNumDeviceCaps;
} __packed;

/* Configuration descriptor */
struct usb_config_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__le16 wTotalLength;
	__u8  bNumInterfaces;
	__u8  bConfigurationValue;
	__u8  iConfiguration;
	__u8  bmAttributes;
	__u8  bMaxPower;
} __packed;

#define USB_DT_CONFIG_SIZE	9

/* Status type (for GET_STATUS) */
#define USB_STATUS_SELFPOWERED	0x0001
#define USB_STATUS_REMOTEWAKEUP	0x0002

/* Helpers */
static inline int usb_endpoint_num(const struct usb_endpoint_descriptor *epd)
{
	return epd->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
}

static inline int usb_endpoint_type(const struct usb_endpoint_descriptor *epd)
{
	return epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
}

static inline int usb_endpoint_dir_in(const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN);
}

static inline int usb_endpoint_dir_out(const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT);
}

static inline int usb_endpoint_xfer_bulk(const struct usb_endpoint_descriptor *epd)
{
	return usb_endpoint_type(epd) == USB_ENDPOINT_XFER_BULK;
}

static inline int usb_endpoint_xfer_int(const struct usb_endpoint_descriptor *epd)
{
	return usb_endpoint_type(epd) == USB_ENDPOINT_XFER_INT;
}

static inline int usb_endpoint_xfer_isoc(const struct usb_endpoint_descriptor *epd)
{
	return usb_endpoint_type(epd) == USB_ENDPOINT_XFER_ISOC;
}

static inline int usb_endpoint_xfer_control(const struct usb_endpoint_descriptor *epd)
{
	return usb_endpoint_type(epd) == USB_ENDPOINT_XFER_CONTROL;
}

#endif /* _LINUX_USB_CH9_H */
