#ifndef _LINUX_USB_GADGET_H
#define _LINUX_USB_GADGET_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/usb/ch9.h>

typedef int gfp_t;

struct usb_ep;
struct usb_request;
struct usb_gadget;

/* Completion callback for async I/O */
typedef void (*usb_request_complete_t)(struct usb_ep *ep,
				       struct usb_request *req);

/**
 * struct usb_request - USB gadget I/O request
 */
struct usb_request {
	void			*buf;
	unsigned		length;
	dma_addr_t		dma;

	unsigned		stream_id:16;
	unsigned		no_interrupt:1;
	unsigned		zero:1;
	unsigned		short_not_ok:1;

	usb_request_complete_t	complete;
	void			*context;
	struct list_head	list;

	int			status;
	unsigned		actual;
};

/**
 * struct usb_ep_ops - endpoint operations
 */
struct usb_ep_ops {
	int (*enable)(struct usb_ep *ep,
		      const struct usb_endpoint_descriptor *desc);
	int (*disable)(struct usb_ep *ep);

	struct usb_request *(*alloc_request)(struct usb_ep *ep, gfp_t gfp_flags);
	void (*free_request)(struct usb_ep *ep, struct usb_request *req);

	int (*queue)(struct usb_ep *ep, struct usb_request *req, gfp_t gfp_flags);
	int (*dequeue)(struct usb_ep *ep, struct usb_request *req);

	int (*set_halt)(struct usb_ep *ep, int value);
	int (*set_wedge)(struct usb_ep *ep);

	int (*fifo_status)(struct usb_ep *ep);
	void (*fifo_flush)(struct usb_ep *ep);
};

/**
 * struct usb_ep - USB endpoint
 */
struct usb_ep {
	const char		*name;
	const struct usb_ep_ops	*ops;
	struct list_head	ep_list;
	unsigned		maxpacket:16;
	unsigned		maxpacket_limit:16;
	unsigned		max_streams:16;
	unsigned		mult:2;
	u8			address;
	const struct usb_endpoint_descriptor	*desc;
	const struct usb_ss_ep_comp_descriptor	*comp_desc;
};

/**
 * struct usb_gadget_ops - gadget operations
 */
struct usb_gadget_ops {
	int (*get_frame)(struct usb_gadget *);
	int (*wakeup)(struct usb_gadget *);
	int (*set_selfpowered)(struct usb_gadget *, int is_selfpowered);
	int (*vbus_session)(struct usb_gadget *, int is_active);
	int (*vbus_draw)(struct usb_gadget *, unsigned mA);
	int (*pullup)(struct usb_gadget *, int is_on);
	int (*ioctl)(struct usb_gadget *, unsigned code, unsigned long param);
	int (*udc_start)(struct usb_gadget *,
			 struct usb_gadget_driver *);
	int (*udc_stop)(struct usb_gadget *);
};

/**
 * struct usb_gadget - USB slave device
 */
struct usb_gadget {
	const struct usb_gadget_ops	*ops;
	struct usb_ep			*ep0;
	struct list_head		ep_list;
	enum usb_device_speed		speed;
	enum usb_device_speed		max_speed;
	enum usb_device_state		state;
	const char			*name;

	unsigned			sg_supported:1;
	unsigned			is_otg:1;
	unsigned			is_a_peripheral:1;
	unsigned			b_hnp_enable:1;
	unsigned			a_hnp_support:1;
	unsigned			a_alt_hnp_support:1;
	unsigned			is_selfpowered:1;
	unsigned			quirk_ep_out_aligned_size:1;
	unsigned			quirk_altset_not_supp:1;
	unsigned			quirk_stall_not_supp:1;
	unsigned			quirk_zlp_not_supp:1;
};

/**
 * struct usb_gadget_driver - USB gadget class driver
 */
struct usb_gadget_driver {
	char			*function;
	enum usb_device_speed	max_speed;
	int			(*bind)(struct usb_gadget *gadget,
					struct usb_gadget_driver *driver);
	void			(*unbind)(struct usb_gadget *);
	int			(*setup)(struct usb_gadget *,
					 const struct usb_ctrlrequest *);
	void			(*disconnect)(struct usb_gadget *);
	void			(*suspend)(struct usb_gadget *);
	void			(*resume)(struct usb_gadget *);
};

/* Helpers */
static inline void usb_ep_set_maxpacket_limit(struct usb_ep *ep,
					       unsigned maxpacket_limit)
{
	ep->maxpacket = ep->maxpacket_limit = maxpacket_limit;
}

static inline struct usb_request *usb_ep_alloc_request(struct usb_ep *ep,
						       gfp_t gfp_flags)
{
	return ep->ops->alloc_request(ep, gfp_flags);
}

static inline void usb_ep_free_request(struct usb_ep *ep,
				       struct usb_request *req)
{
	ep->ops->free_request(ep, req);
}

static inline int usb_ep_queue(struct usb_ep *ep, struct usb_request *req,
			       gfp_t gfp_flags)
{
	return ep->ops->queue(ep, req, gfp_flags);
}

static inline int usb_gadget_map_request(struct usb_gadget *gadget,
					 struct usb_request *req, int is_in)
{
	/* Identity-mapped bare-metal: DMA address == virtual address */
	req->dma = (dma_addr_t)(uintptr_t)req->buf;
	return 0;
}

static inline void usb_gadget_unmap_request(struct usb_gadget *gadget,
					    struct usb_request *req, int is_in)
{
	(void)gadget; (void)req; (void)is_in;
}

static inline void usb_gadget_set_state(struct usb_gadget *gadget,
					enum usb_device_state state)
{
	gadget->state = state;
}

#endif /* _LINUX_USB_GADGET_H */
