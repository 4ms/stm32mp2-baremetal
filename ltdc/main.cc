#include "aarch64/system_reg.hh" // clean_dcache_range
#include "drivers/hal_cnt.hh"	 // SystemA35_SYSTICK_Config, udelay
#include "drivers/pin.hh"
#include "ltdc.hh"
#include "lvds.hh"
#include "panel_etml0700z9.hh"
#include "print/print.hh"
#include "stm32mp2xx.h"
#include <cstdint>
#include <span>

// =============================================================================
//  LTDC + LVDS example -- put a framebuffer on the B-LVDS7-WSVGA panel
// =============================================================================
//
// Display pipeline:   framebuffer (DDR) -> LTDC (scanout) -> LVDS PHY -> panel
//
// Panel: EDT ETML0700Z9NDHA, 1024x600@60, vesa-24 (timings in
// panel_etml0700z9.hh). The LVDS PHY's PLL *generates* the pixel clock (54 MHz
// = 378 MHz bit clock / 7), which is routed back to the LTDC through a SYSCFG
// mux -- so the bring-up order is forced: LVDS PLL first, then LTDC.
//
// Recipe extracted from ST's v6.6 kernel (drivers/gpu/drm/stm/lvds.c, ltdc.c,
// clk-stm32mp25*.c); see ltdc.cc / lvds.cc for the per-register citations.

namespace
{

constexpr uint32_t FbAddr = 0x90000000; // DDR (same pool region the gpu example uses)

// A test pattern that makes scanout bugs obvious: RGB gradient field, 1px
// white border (offset/timing errors show as a missing/wrapped edge), and the
// x^y hash in green (stride errors scramble it visibly).
void fill_test_pattern(std::span<uint32_t> fb)
{
	using namespace Panel;
	for (uint32_t y = 0; y < VActive; y++)
		for (uint32_t x = 0; x < HActive; x++) {
			uint32_t r = (x * 255) / (HActive - 1);
			uint32_t b = (y * 255) / (VActive - 1);
			uint32_t px = 0xFF000000 | (r << 16) | (((x ^ y) & 0xFF) << 8) | b;
			if (x == 0 || y == 0 || x == HActive - 1 || y == VActive - 1)
				px = 0xFFFFFFFF;
			fb[y * HActive + x] = px;
		}
	clean_dcache_range(reinterpret_cast<void *>(FbAddr), HActive * VActive * 4);
}

// RIF: expose the LTDC + LVDS register ports to our secure CPU context, and
// make the LTDC's DMA reads SECURE so they pass the RISAF DDR firewall (the
// same drill as the GPU -- see gpu/etna.cc and the mp2 RIF notes).
//   RISUP 80 = LTDC common regs, 84 = LVDS, 119 = LTDC layers L1/L2
//   RIMU  11 = LTDC_L1/L2 master
void rif_setup()
{
	RISC->SECCFGR[2] |= (1u << (80 - 64)) | (1u << (84 - 64));
	RISC->SECCFGR[3] |= (1u << (119 - 96));
	auto attr = RIMC->ATTR[11];
	RIMC->ATTR[11] = (attr & ~RIMC_ATTR_CIDSEL) | RIMC_ATTR_MSEC | RIMC_ATTR_MPRIV;
}

// One gate bit each turns on BOTH the bus and kernel clocks (single gate per
// IP on MP25); bit 0 of the same registers is the reset. LVDSCFGR bit 15
// selects the PHY PLL reference = HSE (40 MHz) directly -- no flexgen needed.
void clocks_setup()
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

} // namespace

int main()
{
	print("\nLTDC + LVDS Example\n");
	print("===================\n\n");

	SystemA35_SYSTICK_Config(0);

	auto fb = std::span<uint32_t>{reinterpret_cast<uint32_t *>(FbAddr), Panel::HActive * Panel::VActive};
	fill_test_pattern(fb);
	print("1. Test pattern at 0x", Hex{FbAddr}, " (", Panel::HActive, "x", Panel::VActive, ")\n");

	rif_setup();
	print("2. RIF: LTDC(80,119) + LVDS(84) secure, LTDC master (RIMU 11) MSEC\n");

	clocks_setup();
	print("3. Clocks on, resets pulsed, LVDS PLL ref = HSE 40 MHz\n");
	print("   LVDS version 0x", Hex{lvds_version()}, ", LTDC version 0x", Hex{ltdc_hw_version()}, "\n");

	if (!lvds_pll_init()) {
		print("FAILED: LVDS PHY PLL never locked\n");
		while (true)
			asm volatile("wfe");
	}
	print("4. LVDS PLL locked: 40 MHz x ", Panel::PllMdiv, " / (", Panel::PllNdiv, "*", Panel::PllBdiv,
		  ") = 378 MHz bit clock -> 54 MHz pixel\n");

	SYSCFG->DISPLAYCLKCR = 1; // LTDC pixel clock = clk_pix_lvds (PHY /7 output)
	print("5. SYSCFG display clock mux -> LVDS pixel clock\n");

	ltdc_init(FbAddr);
	print("6. LTDC configured + enabled\n");

	lvds_host_enable();
	print("7. LVDS host on (single link, vesa-24)\n");

	Pin panel_en{GPIO::G, PinNum::_15, PinMode::Output};
	panel_en.high();
	udelay(20'000); // let the panel logic wake before backlight
	Pin backlight{GPIO::I, PinNum::_5, PinMode::Output};
	backlight.high();
	print("8. Panel enabled (PG15), backlight on (PI5)\n");

	// --- verify scanout is alive (works even with the panel disconnected) ----
	uint32_t l0 = ltdc_current_line();
	udelay(3000); // ~1/5 frame
	uint32_t l1 = ltdc_current_line();
	print("\nLTDC line counter: ", l0, " -> ", l1, l0 != l1 ? "  (advancing \\o/)\n" : "  (FROZEN: no pixel clock!)\n");
	print("LTDC ISR flags: 0x", Hex{ltdc_isr()}, "  (bit1 FIFO-warn, bit2 xfer-err, bit3 reloaded, bit6 FIFO-err)\n");

	if (l0 == l1)
		print("\nFAILED: pixel clock dead -- check PLL lock / SYSCFG mux\n");
	else
		print("\nScanout running -- look at the panel!\n");

	while (true)
		asm volatile("wfe");
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", int(line), "\n");
}
