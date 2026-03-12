#pragma once
#include <linux/dma-direction.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate zeroed DMA-coherent memory.
 * Returns virtual pointer; *dma_handle receives the bus address.
 * On identity-mapped bare-metal these are the same value. */
void *dma_alloc_coherent(size_t size, unsigned long *dma_handle);

/* Free DMA-coherent memory (no-op in monotonic allocator). */
void dma_free_coherent(void *ptr);

#ifdef __cplusplus
}
#endif
