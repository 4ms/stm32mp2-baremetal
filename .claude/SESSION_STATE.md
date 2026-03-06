# DWC3 Bare-Metal Adaptor — Session State

Last updated: 2026-03-05

## Project Goal

Create a shim/adaptor layer so U-Boot's DWC3 USB driver (core.c, gadget.c, ep0.c)
can run on AArch64 bare-metal (STM32MP2) without Linux or U-Boot frameworks.

## Repository Layout

```
usb-drd/
├── dwc3/                     # U-Boot DWC3 driver (ported, mostly unmodified)
│   ├── core.c                # Controller init, soft reset, PHY config
│   ├── core.h                # Register map, structs, constants
│   ├── gadget.c              # USB gadget (device) mode
│   ├── gadget.h              # Gadget constants
│   ├── ep0.c                 # Control endpoint handling
│   ├── io.h                  # MMIO + cache helpers (needs U-Boot include replacement)
│   ├── linux-compat.h        # Minimal compat (just dev_WARN)
│   └── dwc3-*.c              # Platform glue (not used for bare-metal)
├── dwc3_baremetal.h          # Main adaptor header (11 sections of shims)
├── dwc3_dma.c                # DMA monotonic bump allocator (DONE)
├── mmu.cc                    # Local copy with NC mapping for DMA pool
├── main.cc                   # Skeleton (HAL_Init + infinite loop)
├── system_init.cc            # Clock/power init
├── startup.s                 # AArch64 boot code
├── linkscript.ld             # Memory layout with .dma_coherent section
├── Makefile                  # Build system
└── stm32mp2xx_hal_conf.h     # HAL config
```

## What's Done

### 1. DWC3 Driver Port
- core.c, gadget.c, ep0.c, core.h, gadget.h copied from U-Boot
- Unmodified so far — modifications go through the adaptor header

### 2. Adaptor Header (dwc3_baremetal.h)
All 11 sections written:
- Section 1:  Platform descriptor (dwc3_platform_t)
- Section 2:  DMA/memory shim — signatures fixed to match U-Boot (2-arg alloc, 1-arg free)
- Section 3:  MMIO access (readl/writel via inline asm)
- Section 4:  Spinlock/mutex (no-ops for single-core)
- Section 5:  Delay/time (udelay/mdelay — declared, NOT YET IMPLEMENTED)
- Section 6:  Workqueue shim (pending-flag mechanism)
- Section 7:  Logging (routes to uart_printf)
- Section 8:  Minimal struct device shim + devm_kzalloc/kzalloc/kfree
- Section 9:  Kernel macros (ALIGN, BIT, GENMASK, container_of, list_head, etc.)
- Section 10: IRQ/interrupt shim (irqreturn_t)
- Section 11: Init entry points (dwc3_baremetal_init/shutdown — declared, NOT IMPLEMENTED)
- Has extern "C" guards for C++ compatibility

### 3. DMA Allocator (dwc3_dma.c) — COMPLETE
- Monotonic bump allocator over 16 KB static pool
- Pool in .dma_coherent linker section
- dma_alloc_coherent(size, &handle) — bump + zero + identity-map handle
- dma_free_coherent(ptr) — no-op
- dma_map_single / dma_unmap_single — identity
- Related macros in header: dma_mapping_error, flush_dcache_range (no-op),
  CONFIG_SYS_CACHELINE_SIZE, ROUND, DIV_ROUND_UP, memalign, kmalloc_array

### 4. MMU Setup (mmu.cc)
- Copied from shared/mmu/mmu.cc to local dir
- Added NC block entry: L1_ddr1.block_entry(0x8A000000, Noncache, AccessRW)
- Makefile updated to use local copy

### 5. Linker Script (linkscript.ld)
- Added DMA_NC memory region at 0x8A000000 (2MB)
- Added .dma_coherent section mapped to DMA_NC

## What's NOT Done Yet

### Must implement before the driver can compile:

1. **U-Boot header substitution** — driver .c files include U-Boot headers that
   don't exist in bare-metal:
   - `<malloc.h>` — provides memalign, dma_alloc_coherent (now in our header)
   - `<cpu_func.h>` — provides flush_dcache_range (now no-op in our header)
   - `<asm/io.h>` — provides readl/writel (now in our header)
   - `<linux/usb/ch9.h>` — USB descriptor structs
   - `<linux/usb/gadget.h>` — gadget framework types
   Options: (a) provide stub headers at those include paths, or (b) modify
   the driver files to include dwc3_baremetal.h instead.

2. **io.h adaptation** — dwc3/io.h includes <cpu_func.h> and <asm/io.h>.
   Needs to include dwc3_baremetal.h instead, or provide those stub headers.
   Also defines dwc3_flush_cache() which calls flush_dcache_range() — our
   header already provides the no-op, but io.h needs to see it.

3. **udelay / mdelay** — declared in header, need implementation.
   Should use AArch64 generic timer (CNTPCT_EL0 / CNTFRQ_EL0).

4. **USB descriptor types** — struct usb_ctrlrequest, struct usb_endpoint_descriptor,
   struct usb_gadget, struct usb_ep, struct usb_request, etc.
   These come from Linux/U-Boot USB headers. Need to provide them.

5. **Makefile** — dwc3 .c files (core.c, gadget.c, ep0.c) are NOT in the
   SOURCES list yet. Need to add them with appropriate CFLAGS.

6. **dwc3_baremetal_init / dwc3_baremetal_shutdown** — declared but not
   implemented. These are the glue that calls dwc3_core_init() and
   dwc3_gadget_init().

### After compilation works:

7. **USB device class** — CDC-ACM, mass storage, or custom class
8. **Interrupt wiring** — connect dwc3_interrupt to GIC (or use polling)
9. **Testing on hardware**

## Key Architecture Decisions

- **Non-Cacheable DMA**: All DMA buffers in NC-mapped DDR (MAIR 0x44).
  No explicit cache flushes needed. dwc3_flush_cache is a no-op.
- **Identity mapping**: Virtual == physical for DMA addresses.
- **Monotonic allocator**: No free() needed — all DMA is device-lifetime.
- **Polling first**: dwc3_gadget_poll() in main loop, interrupt later.
- **No hibernation**: Scratch buffers never allocated.

## DMA Allocation Budget (for reference)

| Allocation       | Size    | Count | Source            |
|-----------------|---------|-------|-------------------|
| Event buffer    | 256 B   | 1     | core.c:148        |
| ctrl_req        | 8 B     | 1     | gadget.c:2567     |
| ep0_trb         | 32 B    | 1     | gadget.c:2575     |
| ep0_bounce      | 512 B   | 1     | gadget.c:2590     |
| TRB pool        | 512 B   | <=15  | gadget.c:346      |
| **Total worst** | ~8.5 KB |       | Pool is 16 KB     |

## Function Signature Reference (U-Boot calling convention)

```c
// DMA — matches callsites in core.c / gadget.c:
void *dma_alloc_coherent(size_t size, unsigned long *dma_handle);  // 2-arg
void  dma_free_coherent(void *ptr);                                // 1-arg
uintptr_t dma_map_single(void *ptr, size_t size, int direction);   // 3-arg
void      dma_unmap_single(uintptr_t dma, size_t size, int dir);   // 3-arg

// Time:
void udelay(unsigned int us);
void mdelay(unsigned int ms);
```

## Build Notes

- Cross-compiler: aarch64-none-elf-gcc (or similar)
- Target: STM32MP257 Cortex-A35
- EL3 secure state by default
- Heap: 256 KB (for malloc/calloc/kzalloc)
- Stacks: 64 KB each (FIQ, IRQ, main)
