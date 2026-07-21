#include "display.hh"
#include "drivers/hal_cnt.hh" // udelay
#include "drivers/pin.hh"
#include "ltdc.hh"
#include "lvds.hh"
#include "stm32mp2xx.h"

// =============================================================================
//  display.cc -- EV1 board wiring for the LVDS display path
// =============================================================================

// RIF: expose the LTDC + LVDS register ports to our secure CPU context, and
// make the LTDC's DMA reads SECURE so they pass the RISAF DDR firewall (the
// same drill as the GPU -- see gpu/etna.cc and the mp2 RIF notes).
//   RISUP 80 = LTDC common regs, 84 = LVDS, 119 = LTDC layers L1/L2
//   RIMU  11 = LTDC_L1/L2 master
void display_rif_setup()
{
	RISC->SECCFGR[2] |= (1u << (80 - 64)) | (1u << (84 - 64));
	RISC->SECCFGR[3] |= (1u << (119 - 96));
	auto attr = RIMC->ATTR[11];
	RIMC->ATTR[11] = (attr & ~RIMC_ATTR_CIDSEL) | RIMC_ATTR_MSEC | RIMC_ATTR_MPRIV;
}

// One gate bit each turns on BOTH the bus and kernel clocks (single gate per
// IP on MP25); bit 0 of the same registers is the reset. LVDSCFGR bit 15
// selects the PHY PLL reference = HSE (40 MHz) directly -- no flexgen needed.
void display_clocks_setup()
{
	RCC->LTDCCFGR |= 1u << 1;
	RCC->LVDSCFGR |= 1u << 1;

	RCC->LTDCCFGR |= 1u << 0; // pulse resets
	RCC->LVDSCFGR |= 1u << 0;
	udelay(20);
	RCC->LTDCCFGR &= ~(1u << 0);
	RCC->LVDSCFGR &= ~(1u << 0);

	RCC->LVDSCFGR |= 1u << 15; // PHY ref = HSE 40 MHz
}

void display_panel_on()
{
	Pin panel_en{GPIO::G, PinNum::_15, PinMode::Output};
	panel_en.high();
	udelay(20'000); // let the panel logic wake before backlight
	Pin backlight{GPIO::I, PinNum::_5, PinMode::Output};
	backlight.high();
}

bool display_init(uint32_t first_fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg_argb)
{
	display_rif_setup();
	display_clocks_setup();
	if (!lvds_pll_init())
		return false;
	SYSCFG->DISPLAYCLKCR = 1; // LTDC pixel clock = clk_pix_lvds (PHY /7 output)
	ltdc_init(first_fb, x, y, w, h, bg_argb);
	lvds_host_enable();
	display_panel_on();
	return true;
}
