#ifndef _LINUX_USB_OTG_H
#define _LINUX_USB_OTG_H

/* USB dual-role mode */
enum usb_dr_mode {
	USB_DR_MODE_UNKNOWN = 0,
	USB_DR_MODE_HOST,
	USB_DR_MODE_PERIPHERAL,
	USB_DR_MODE_OTG,
};

#endif /* _LINUX_USB_OTG_H */
