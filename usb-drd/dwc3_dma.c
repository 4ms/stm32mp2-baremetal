/* dwc3_dma.c - Monotonic bump allocator for DWC3 DMA-coherent memory
 *
 * This allocator is deliberately trivial: all DWC3 DMA allocations
 * happen at init time and live for the lifetime of the controller.
 * A bump pointer over a static Non-Cacheable buffer is all we need.
 *
 * Memory placement
 * ----------------
 * The pool buffer lives in the .dma_coherent linker section.
 * You must:
 *   1. Place .dma_coherent at a 2MB-aligned DDR address in your
 *      linker script (e.g. 0x8A000000).
 *   2. Map that 2MB block as Non-Cacheable in your MMU tables:
 *        L1_ddr1.block_entry(0x8A000000,
 *                            MMU::MemType::Noncache,
 *                            MMU::AccessRW);
 *      (after the fill_block_entries call that sets DDR to Normal)
 *
 * Pool budget
 * -----------
 * DWC3 gadget-mode allocations at init:
 *   event buffer     256 B   (4 * 64 events, typically 1 buffer)
 *   ctrl_req           8 B   (struct usb_ctrlrequest)
 *   ep0_trb           32 B   (2 x dwc3_trb)
 *   ep0_bounce       512 B   (DWC3_EP0_BOUNCE_SIZE)
 *   TRB pools     15*512 B   (up to 15 non-EP0 endpoints, 32 TRBs each)
 *   ─────────────────────
 *   total          ~8.5 KB   worst case
 *
 * 16 KB pool gives comfortable headroom.
 */

#include "dwc3_baremetal.h"
#include <string.h>

#define DWC3_DMA_POOL_SIZE (16u * 1024u)

/* Minimum alignment for all DMA allocations.
 * DWC3 TRBs require 16-byte alignment; we round up to 64 to match
 * typical cache-line size (harmless on NC memory, keeps hardware happy). */
#define DWC3_DMA_ALIGN 64

static uint8_t __attribute__((section(".dma_coherent"), aligned(DWC3_DMA_ALIGN)))
	dma_pool[DWC3_DMA_POOL_SIZE];

static size_t dma_pool_offset;

/* ── DMA-coherent allocation ──────────────────────────────────── */

void *dma_alloc_coherent(size_t size, unsigned long *dma_handle)
{
	/* Align offset up */
	dma_pool_offset = (dma_pool_offset + (DWC3_DMA_ALIGN - 1))
			  & ~((size_t)DWC3_DMA_ALIGN - 1);

	if (dma_pool_offset + size > DWC3_DMA_POOL_SIZE) {
		dev_err(NULL, "DMA pool exhausted (need %zu, have %zu)\n",
			size, DWC3_DMA_POOL_SIZE - dma_pool_offset);
		return NULL;
	}

	void *ptr = &dma_pool[dma_pool_offset];
	dma_pool_offset += size;

	memset(ptr, 0, size);

	/* Identity-mapped: physical == virtual */
	if (dma_handle)
		*dma_handle = (unsigned long)ptr;

	return ptr;
}

void dma_free_coherent(void *ptr)
{
	/* Monotonic allocator: no-op.
	 * All DMA memory lives for the lifetime of the controller. */
	(void)ptr;
}

/* ── Streaming DMA mapping (identity) ─────────────────────────── */

uintptr_t dma_map_single(void *ptr, size_t size, int direction)
{
	/* NC memory + identity map: nothing to flush, address is 1:1 */
	(void)size;
	(void)direction;
	return (uintptr_t)ptr;
}

void dma_unmap_single(uintptr_t dma, size_t size, int direction)
{
	(void)dma;
	(void)size;
	(void)direction;
}
