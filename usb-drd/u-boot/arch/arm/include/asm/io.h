#ifndef _ASM_IO_H
#define _ASM_IO_H

#include <linux/compiler.h>
#include <linux/types.h>

static inline u32 readl(const volatile void __iomem *addr)
{
	u32 val;
	__asm__ volatile("ldr %w0, [%1]" : "=r"(val) : "r"(addr) : "memory");
	return val;
}

static inline void writel(u32 val, volatile void __iomem *addr)
{
	__asm__ volatile("str %w0, [%1]" : : "r"(val), "r"(addr) : "memory");
}

#define readl_relaxed(addr) readl(addr)
#define writel_relaxed(val, addr) writel(val, addr)

#endif /* _ASM_IO_H */
