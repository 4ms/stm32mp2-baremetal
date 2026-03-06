#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Map a normal buffer for DMA — identity on NC memory. */
static inline uintptr_t dma_map_single(void *ptr, size_t size, int direction)
{
	/* NC memory + identity map: nothing to flush, address is 1:1 */
	(void)size;
	(void)direction;
	return (uintptr_t)ptr;
}

static inline void dma_unmap_single(uintptr_t dma, size_t size, int direction)
{
	(void)dma;
	(void)size;
	(void)direction;
}

/* dma_mapping_error — always succeeds with identity mapping */
#define dma_mapping_error(dev, addr) (0)

#ifdef __cplusplus
}
#endif
