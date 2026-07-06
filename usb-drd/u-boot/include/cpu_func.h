#ifndef _CPU_FUNC_H
#define _CPU_FUNC_H

#include <stdint.h>

#define CONFIG_SYS_CACHELINE_SIZE	64

/* AArch64 data cache maintenance by VA.
 * These operate on cacheable memory (e.g. heap-allocated DMA buffers).
 * For non-cacheable regions (.dma_coherent), these are harmless no-ops. */

static inline void flush_dcache_range(uintptr_t start, uintptr_t end)
{
	uintptr_t addr;
	const uintptr_t line = CONFIG_SYS_CACHELINE_SIZE;

	start &= ~(line - 1);
	for (addr = start; addr < end; addr += line)
		__asm__ volatile("dc civac, %0" :: "r"(addr) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");
}

static inline void invalidate_dcache_range(uintptr_t start, uintptr_t end)
{
	uintptr_t addr;
	const uintptr_t line = CONFIG_SYS_CACHELINE_SIZE;

	start &= ~(line - 1);
	for (addr = start; addr < end; addr += line)
		__asm__ volatile("dc ivac, %0" :: "r"(addr) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");
}

#endif /* _CPU_FUNC_H */
