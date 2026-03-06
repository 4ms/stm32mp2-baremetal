/* dwc3_baremetal.h
 *
 * Minimal platform struct and kernel shim for bare-metal DWC3 port.
 * Based on U-Boot's DWC3 driver, stripped further for AArch64 bare-metal.
 *
 * Strategy:
 *   - dwc3_platform_t  : your hardware description (replaces struct udevice)
 *   - linux_compat.h   : typedefs and stubs to satisfy core.h / gadget.c includes
 *   - Cache coherency  : handled via Non-Cacheable MAIR mapping, no explicit flushes
 *   - Event loop       : polling to start, interrupt hookup added later
 */

#ifndef DWC3_BAREMETAL_H
#define DWC3_BAREMETAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * SECTION 1: Platform descriptor
 * Replaces Linux's struct device / struct platform_device /
 * struct udevice. Only what the DWC3 driver actually needs.
 * ============================================================ */

typedef enum {
	DWC3_DR_MODE_PERIPHERAL = 0,
	DWC3_DR_MODE_HOST,
	DWC3_DR_MODE_OTG, /* static OTG — pick one at init */
} dwc3_dr_mode_t;

typedef struct {
	/* --- Hardware location --- */
	uintptr_t regs_base; /* MMIO base address of DWC3 core */
	size_t regs_size;	 /* MMIO region size (typically 0x10000) */
	int irq;			 /* IRQ number, -1 if polling only */

	/* --- Clocks (call your own clock enable before dwc3_init) --- */
	/* Nothing here — handle clocks externally before calling dwc3_init().
	 * DWC3 core.c only touches GCTL for soft-reset; it doesn't manage clocks
	 * itself. U-Boot does the same — clock setup is in the board/glue layer. */

	/* --- PHY --- */
	/* Similarly, PHY power-up is done externally. DWC3 core.c handles
	 * GUSB2PHYCFG/GUSB3PIPECTL resets only. */

	/* --- Mode --- */
	dwc3_dr_mode_t dr_mode; /* static mode selection */

	/* --- Quirks (add as needed for your specific DWC3 instance) --- */
	bool dis_u2_susphy_quirk; /* disable USB2 PHY suspend */
	bool dis_enblslpm_quirk;  /* disable ENBLSLPM in GUSB2PHYCFG */
	bool ref_clk_per_quirk;	  /* ref_clk_per field in GUCTL */
	uint32_t ref_clk_per;	  /* value if above quirk is set */

	/* --- USB speed cap --- */
	/* DWC3_DCFG_HIGHSPEED / DWC3_DCFG_FULLSPEED / DWC3_DCFG_LOWSPEED */
	uint8_t maximum_speed;

} dwc3_platform_t;

/* ============================================================
 * SECTION 2: DMA / memory allocation shim
 *
 * DWC3 needs DMA-coherent memory for:
 *   - TRB rings   (struct dwc3_trb[])
 *   - Event buffers (struct dwc3_event_buffer)
 *   - Scratchpad (only if hibernation — not used in bare-metal)
 *
 * On AArch64 bare-metal with DMA buffers mapped as
 * Normal Non-Cacheable (MAIR index 0x44), coherency
 * is automatic — no flush/invalidate needed.
 *
 * Implementation: dwc3_dma.c provides a monotonic bump allocator
 * over a static pool in the .dma_coherent linker section.
 *
 * Signatures below match the U-Boot calling convention used
 * throughout core.c / gadget.c / ep0.c.
 * ============================================================ */
#include <linux/dma-mapping.h>

/* Cache maintenance — no-op when DMA buffers are Non-Cacheable.
 * dwc3_flush_cache() is called from io.h; we override it here so
 * the driver's io.h does not need its U-Boot includes. */
#define CONFIG_SYS_CACHELINE_SIZE 64
#define CACHELINE_SIZE CONFIG_SYS_CACHELINE_SIZE
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
#define ROUND(a, b) (((a) + (b) - 1) & ~((b) - 1))
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

/* memalign — used by core.c for ev_buffs array and by gadget.c
 * for setup_buf.  These are regular (non-DMA) allocations.
 * Newlib provides memalign(); declare it here so the driver
 * doesn't need U-Boot's <malloc.h>.  The returned pointer is
 * free()-compatible. */
void *memalign(size_t alignment, size_t size);

/* kmalloc_array — only used for scratch buffers (hibernation),
 * which we never enable.  Provide it for link completeness. */
#define kmalloc_array(n, size, flags) calloc((n), (size))

/* ============================================================
 * SECTION 3: MMIO access shim
 *
 * On AArch64, readl/writel are just word-width accesses with
 * a compiler barrier. The _relaxed variants skip the DSB.
 * These match Linux semantics closely enough for DWC3.
 * ============================================================ */

static inline uint32_t readl(const volatile void *addr)
{
	uint32_t val;
	__asm__ volatile("ldr %w0, [%1]" : "=r"(val) : "r"(addr) : "memory");
	return val;
}

static inline void writel(uint32_t val, volatile void *addr)
{
	__asm__ volatile("str %w0, [%1]" : : "r"(val), "r"(addr) : "memory");
}

/* Relaxed variants — no barrier, fine for most register sequences */
#define readl_relaxed(addr) readl(addr)
#define writel_relaxed(val, addr) writel(val, addr)

/* Read-modify-write helpers */
static inline void dwc3_setbits(volatile void *addr, uint32_t mask)
{
	writel(readl(addr) | mask, addr);
}
static inline void dwc3_clrbits(volatile void *addr, uint32_t mask)
{
	writel(readl(addr) & ~mask, addr);
}

/* ============================================================
 * SECTION 4: Spinlock / mutex shim
 *
 * On a single-core bare-metal system these are no-ops.
 * On multi-core you'd replace with your real primitives.
 * ============================================================ */

typedef struct {
} spinlock_t;
typedef struct {
} mutex_t;

typedef uint32_t irqflags_t;

#define spin_lock_init(l)                                                                                              \
	do {                                                                                                               \
	} while (0)
#define spin_lock_irqsave(l, flags)                                                                                    \
	do {                                                                                                               \
		(void)(l);                                                                                                     \
		(flags) = 0;                                                                                                   \
	} while (0)
#define spin_unlock_irqrestore(l, flags)                                                                               \
	do {                                                                                                               \
		(void)(l);                                                                                                     \
		(void)(flags);                                                                                                 \
	} while (0)
#define spin_lock(l)                                                                                                   \
	do {                                                                                                               \
	} while (0)
#define spin_unlock(l)                                                                                                 \
	do {                                                                                                               \
	} while (0)

#define mutex_init(m)                                                                                                  \
	do {                                                                                                               \
	} while (0)
#define mutex_lock(m)                                                                                                  \
	do {                                                                                                               \
	} while (0)
#define mutex_unlock(m)                                                                                                \
	do {                                                                                                               \
	} while (0)

/* ============================================================
 * SECTION 5: Delay / time shim
 *
 * DWC3 core.c uses these during reset sequences.
 * Provide real implementations backed by a hardware timer.
 * ============================================================ */

/* Busy-wait microseconds */
void udelay(unsigned int us);

/* Busy-wait milliseconds */
void mdelay(unsigned int ms);

/* Timeout helper used in dwc3_core_soft_reset() polling loops */
static inline int dwc3_poll_register(volatile void *addr, uint32_t mask, uint32_t expected, unsigned int timeout_us)
{
	while (timeout_us--) {
		if ((readl(addr) & mask) == expected)
			return 0;
		udelay(1);
	}
	return -1; /* timeout */
}

/* ============================================================
 * SECTION 6: Workqueue shim (for drd.c role switching)
 *
 * Linux drd.c uses schedule_work() to defer role switches.
 * In bare-metal we handle this as a simple pending-flag that
 * you service in your main loop or on a timer tick.
 * ============================================================ */

typedef struct work_struct {
	void (*func)(struct work_struct *work);
	int pending;
} work_struct_t;

#define INIT_WORK(w, f)                                                                                                \
	do {                                                                                                               \
		(w)->func = (f);                                                                                               \
		(w)->pending = 0;                                                                                              \
	} while (0)
#define schedule_work(w)                                                                                               \
	do {                                                                                                               \
		(w)->pending = 1;                                                                                              \
	} while (0)

/* Call this in your main loop to drain pending work */
static inline void run_pending_work(work_struct_t *w)
{
	if (w->pending) {
		w->pending = 0;
		w->func(w);
	}
}

/* ============================================================
 * SECTION 7: Logging shim
 *
 * Route to your UART debug output or discard entirely.
 * ============================================================ */

#include <stdio.h>

// extern int uart_printf(const char *fmt, ...);
#define uart_printf printf

#define dev_dbg(dev, fmt, ...) uart_printf("[DWC3 DBG] " fmt, ##__VA_ARGS__)
#define dev_info(dev, fmt, ...) uart_printf("[DWC3 INF] " fmt, ##__VA_ARGS__)
#define dev_warn(dev, fmt, ...) uart_printf("[DWC3 WRN] " fmt, ##__VA_ARGS__)
#define dev_err(dev, fmt, ...) uart_printf("[DWC3 ERR] " fmt, ##__VA_ARGS__)

/* pr_* variants used in some files */
#define pr_debug(fmt, ...) uart_printf("[DWC3 DBG] " fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...) uart_printf("[DWC3 INF] " fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...) uart_printf("[DWC3 WRN] " fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) uart_printf("[DWC3 ERR] " fmt, ##__VA_ARGS__)

/* WARN_ON — assert-style, keep as a no-op or add your abort handler */
#define WARN_ON(cond) ((void)(cond))
#define WARN_ON_ONCE(cond) ((void)(cond))
#define BUG_ON(cond)                                                                                                   \
	do {                                                                                                               \
		if (cond) {                                                                                                    \
			uart_printf("BUG_ON at %s:%d\n", __FILE__, __LINE__);                                                      \
			while (1)                                                                                                  \
				;                                                                                                      \
		}                                                                                                              \
	} while (0)

/* ============================================================
 * SECTION 8: Minimal struct device shim
 *
 * DWC3 core.h embeds a `struct device *dev` in struct dwc3.
 * We need just enough to satisfy:
 *   - dev_err/dev_info (handled by macros above — dev arg ignored)
 *   - devm_kzalloc (we redirect to plain malloc)
 *   - dma_alloc_coherent(dwc->dev, ...) (dev arg ignored)
 * ============================================================ */

struct device {
	const char *name;  /* optional, for debug output */
	void *driver_data; /* dwc3 stores itself here */
};

/* devm_* — "device managed" allocs. In bare-metal we just malloc.
 * Nothing calls devm_free explicitly anyway — lifetime is the whole app. */
#include <stdlib.h>
#define devm_kzalloc(dev, size, flags) calloc(1, size)
#define devm_kfree(dev, ptr) free(ptr)
#define kzalloc(size, flags) calloc(1, size)
#define kmalloc(size, flags) malloc(size)
#define kfree(ptr) free(ptr)

/* GFP flags — Linux memory allocation flags, ignored in bare-metal */
#define GFP_KERNEL 0
#define GFP_ATOMIC 0

/* dev_set_drvdata / dev_get_drvdata */
#define dev_set_drvdata(dev, data) ((dev)->driver_data = (data))
#define dev_get_drvdata(dev) ((dev)->driver_data)

/* ============================================================
 * SECTION 9: Miscellaneous kernel macros used in DWC3
 * ============================================================ */

/* Alignment */
#define ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)

/* Min/max */
#define min_t(type, a, b) ((type)(a) < (type)(b) ? (type)(a) : (type)(b))
#define max_t(type, a, b) ((type)(a) > (type)(b) ? (type)(a) : (type)(b))
#define clamp(val, lo, hi) min_t(__typeof__(val), max_t(__typeof__(val), val, lo), hi)

/* Bit operations */
#define BIT(n) (1UL << (n))
#define GENMASK(h, l) (((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define BITS_PER_LONG 64

/* Field extract/insert (used in register manipulation) */
#define FIELD_GET(mask, reg) (((reg) & (mask)) >> __builtin_ctzl(mask))
#define FIELD_PREP(mask, val) (((val) << __builtin_ctzl(mask)) & (mask))

/* Array size */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Container of */
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

/* List head — DWC3 uses these for pending_list / started_list on endpoints */
struct list_head {
	struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) {&(name), &(name)}
#define INIT_LIST_HEAD(ptr)                                                                                            \
	do {                                                                                                               \
		(ptr)->next = (ptr);                                                                                           \
		(ptr)->prev = (ptr);                                                                                           \
	} while (0)

static inline void list_add_tail(struct list_head *newnode, struct list_head *head)
{
	newnode->next = head;
	newnode->prev = head->prev;
	head->prev->next = newnode;
	head->prev = newnode;
}

static inline void list_del(struct list_head *entry)
{
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
	entry->next = entry->prev = NULL;
}

static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_for_each_entry(pos, head, member)                                                                         \
	for (pos = list_entry((head)->next, __typeof__(*pos), member); &pos->member != (head);                             \
		 pos = list_entry(pos->member.next, __typeof__(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member)                                                                 \
	for (pos = list_entry((head)->next, __typeof__(*pos), member),                                                     \
		n = list_entry(pos->member.next, __typeof__(*pos), member);                                                    \
		 &pos->member != (head);                                                                                       \
		 pos = n, n = list_entry(n->member.next, __typeof__(*pos), member))

/* ERR_PTR / IS_ERR / PTR_ERR — used in gadget.c endpoint alloc */
#define MAX_ERRNO 4095
#define IS_ERR_VALUE(x) ((unsigned long)(x) >= (unsigned long)-MAX_ERRNO)
#define ERR_PTR(err) ((void *)(long)(err))
#define PTR_ERR(ptr) ((long)(ptr))
#define IS_ERR(ptr) IS_ERR_VALUE((unsigned long)(ptr))

/* Compiler hints */
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define __maybe_unused __attribute__((unused))
#ifndef __packed
#define __packed __attribute__((packed))
#endif

/* ============================================================
 * SECTION 10: IRQ / interrupt shim
 *
 * DWC3's interrupt handler signature:
 *   irqreturn_t dwc3_interrupt(int irq, void *_dwc);
 *
 * Wire this to your bare-metal IRQ vector directly.
 * ============================================================ */

typedef enum {
	IRQ_NONE = 0,
	IRQ_HANDLED = 1,
} irqreturn_t;

/* In your IRQ vector: */
/*
	extern irqreturn_t dwc3_interrupt(int irq, void *_dwc);
	void your_usb_irq_handler(void) {
		dwc3_interrupt(DWC3_IRQ_NUM, your_dwc3_instance);
	}
*/

/* For polling mode (no IRQ), call this from your main loop: */
/*
	extern void dwc3_gadget_poll(struct dwc3 *dwc);
*/

/* ============================================================
 * SECTION 11: Initialization entry point
 *
 * Replace Linux's platform_driver probe() with a direct call.
 * ============================================================ */

struct dwc3; /* forward declaration — defined in core.h */

/*
 * dwc3_baremetal_init - Initialize DWC3 controller
 *
 * Allocates struct dwc3, runs core init sequence, and starts
 * the controller in the mode specified by platform->dr_mode.
 *
 * Returns pointer to initialized dwc3 struct, or NULL on failure.
 *
 * Caller is responsible for:
 *   1. Enabling clocks before calling
 *   2. Powering up PHY before calling
 *   3. Calling dwc3_gadget_poll() in main loop (polling mode)
 *      OR wiring dwc3_interrupt() to IRQ vector (interrupt mode)
 */
struct dwc3 *dwc3_baremetal_init(const dwc3_platform_t *platform);

/*
 * dwc3_baremetal_shutdown - Stop and de-initialize controller
 */
void dwc3_baremetal_shutdown(struct dwc3 *dwc);

#ifdef __cplusplus
}
#endif
#endif /* DWC3_BAREMETAL_H */
