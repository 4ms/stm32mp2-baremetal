#ifndef _CPU_FUNC_H
#define _CPU_FUNC_H

#include <stdint.h>

/* Cache maintenance — no-op when DMA buffers are Non-Cacheable. */

#define CONFIG_SYS_CACHELINE_SIZE	64

static inline void flush_dcache_range(uintptr_t start, uintptr_t end)
{
	(void)start;
	(void)end;
}

static inline void invalidate_dcache_range(uintptr_t start, uintptr_t end)
{
	(void)start;
	(void)end;
}

#endif /* _CPU_FUNC_H */
