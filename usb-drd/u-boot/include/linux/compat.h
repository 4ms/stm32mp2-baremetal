/* Bare-metal shim for linux/compat.h — minimal subset needed by xHCI */
#ifndef _LINUX_COMPAT_H_
#define _LINUX_COMPAT_H_

#include <stdlib.h>
#include <string.h>
#include <linux/types.h>
#include <linux/kernel.h>

/* GFP flags — unused in bare-metal, but calloc is fine */
#define GFP_ATOMIC	((gfp_t) 0)
#define GFP_KERNEL	((gfp_t) 0)
#define __GFP_ZERO	((__force gfp_t)0x8000u)

/* Use stdlib malloc/free directly; no wrappers needed.
 * The xHCI code uses memalign() for DMA allocations (via xhci-mem.c). */

#define PAGE_SIZE	4096

/* Timer / scheduling stubs */
#ifndef schedule
#define schedule()	do {} while (0)
#endif

#endif
