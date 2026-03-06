#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H

#include <stddef.h>
#include <linux/compiler.h>
#include <linux/types.h>

#define ALIGN(x, a)		(((x) + ((a) - 1)) & ~((a) - 1))
#define IS_ALIGNED(x, a)	(((x) & ((a) - 1)) == 0)

#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))

#define DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))
#define ROUND(a, b)		(((a) + (b) - 1) & ~((b) - 1))

#define min_t(type, a, b)	((type)(a) < (type)(b) ? (type)(a) : (type)(b))
#define max_t(type, a, b)	((type)(a) > (type)(b) ? (type)(a) : (type)(b))
#define clamp(val, lo, hi)	min_t(__typeof__(val), max_t(__typeof__(val), val, lo), hi)

#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))

#define upper_32_bits(n)	((u32)(((n) >> 16) >> 16))
#define lower_32_bits(n)	((u32)(n))

#endif /* _LINUX_KERNEL_H */
