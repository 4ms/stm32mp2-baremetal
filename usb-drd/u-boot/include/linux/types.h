#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef u8 __u8;
typedef u16 __u16;
typedef u32 __u32;
typedef u64 __u64;

typedef u16 __le16;
typedef u32 __le32;
typedef u64 __le64;

typedef unsigned long phys_addr_t;
typedef unsigned long dma_addr_t;
typedef unsigned long resource_size_t;

#endif /* _LINUX_TYPES_H */
