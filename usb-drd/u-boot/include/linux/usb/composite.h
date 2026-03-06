#ifndef _LINUX_USB_COMPOSITE_H
#define _LINUX_USB_COMPOSITE_H

/* Stub — ep0.c includes this but only uses types from ch9.h and gadget.h. */

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>


/*
 * USB function drivers should return USB_GADGET_DELAYED_STATUS if they
 * wish to delay the data/status stages of the control transfer till they
 * are ready. The control transfer will then be kept from completing till
 * all the function drivers that requested for USB_GADGET_DELAYED_STAUS
 * invoke usb_composite_setup_continue().
 */
#define	USB_GADGET_DELAYED_STATUS	0x7fff /* Impossibly large value */


#endif /* _LINUX_USB_COMPOSITE_H */
