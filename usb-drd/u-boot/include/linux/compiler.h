#ifndef _LINUX_COMPILER_H
#define _LINUX_COMPILER_H

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define __maybe_unused	__attribute__((unused))

#ifndef __packed
#define __packed	__attribute__((packed))
#endif

#define __iomem

#endif /* _LINUX_COMPILER_H */
