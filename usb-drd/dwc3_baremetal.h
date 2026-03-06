/* dwc3_baremetal.h
 *
 * Bare-metal platform descriptor and init entry points for DWC3.
 *
 * The kernel/U-Boot shims that the DWC3 driver needs are now provided
 * by individual stub headers under u-boot/include/ and
 * u-boot/arch/arm/include/, matching the U-Boot directory layout.
 *
 * This file contains only the custom bare-metal pieces:
 *   - dwc3_platform_t  : your hardware description
 *   - dwc3_baremetal_init/shutdown : initialization entry points
 */

#ifndef DWC3_BAREMETAL_H
#define DWC3_BAREMETAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Platform descriptor
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
 * Initialization entry point
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

/* ============================================================
 * Timeout helper used in dwc3_core_soft_reset() polling loops
 * ============================================================ */
#include <linux/delay.h>
#include <asm/io.h>

static inline int dwc3_poll_register(volatile void *addr, uint32_t mask,
				     uint32_t expected, unsigned int timeout_us)
{
	while (timeout_us--) {
		if ((readl(addr) & mask) == expected)
			return 0;
		udelay(1);
	}
	return -1; /* timeout */
}

#ifdef __cplusplus
}
#endif
#endif /* DWC3_BAREMETAL_H */
