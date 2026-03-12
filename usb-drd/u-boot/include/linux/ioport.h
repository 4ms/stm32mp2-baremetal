#ifndef _LINUX_IOPORT_H
#define _LINUX_IOPORT_H

#include <linux/types.h>

/* Minimal struct resource — DWC3 core.h uses this for xhci_resources[] */
struct resource {
	resource_size_t start;
	resource_size_t end;
	unsigned long flags;
};

#define IORESOURCE_IRQ	0x00000400
#define IORESOURCE_MEM	0x00000200

#endif /* _LINUX_IOPORT_H */
