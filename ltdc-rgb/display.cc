#include "display.hh"
#include "drivers/hal_cnt.hh" // udelay
#include "drivers/pin.hh"
#include "drivers/rcc_pll.hh"  // PLL4 read-back for the pixel clock
#include "drivers/rcc_xbar.hh" // FlexbarConf
#include "ltdc.hh"
#include "nv3052c.hh"
#include "panel_nv3052c.hh"
#include "stm32mp2xx.h"

namespace
{
constexpr uint8_t AF_LTDC = 13;
constexpr PinDef ltdc_pins[] = {
	{GPIO::F, PinNum::_12, PinAF(AF_LTDC)}, // CLK
	{GPIO::I, PinNum::_7, PinAF(AF_LTDC)},	// HSYNC
	{GPIO::I, PinNum::_6, PinAF(AF_LTDC)},	// VSYNC
	{GPIO::I, PinNum::_5, PinAF(AF_LTDC)},	// DE
	{GPIO::F, PinNum::_14, PinAF(AF_LTDC)}, // R3
	{GPIO::F, PinNum::_15, PinAF(AF_LTDC)}, // R4
	{GPIO::G, PinNum::_5, PinAF(AF_LTDC)},	// R5
	{GPIO::G, PinNum::_6, PinAF(AF_LTDC)},	// R6
	{GPIO::G, PinNum::_7, PinAF(AF_LTDC)},	// R7
	{GPIO::G, PinNum::_8, PinAF(AF_LTDC)},	// G2
	{GPIO::G, PinNum::_9, PinAF(AF_LTDC)},	// G3
	{GPIO::G, PinNum::_10, PinAF(AF_LTDC)}, // G4
	{GPIO::G, PinNum::_11, PinAF(AF_LTDC)}, // G5
	{GPIO::G, PinNum::_12, PinAF(AF_LTDC)}, // G6
	{GPIO::G, PinNum::_13, PinAF(AF_LTDC)}, // G7
	{GPIO::I, PinNum::_0, PinAF(AF_LTDC)},	// B3
	{GPIO::I, PinNum::_1, PinAF(AF_LTDC)},	// B4
	{GPIO::I, PinNum::_2, PinAF(AF_LTDC)},	// B5
	{GPIO::I, PinNum::_3, PinAF(AF_LTDC)},	// B6
	{GPIO::I, PinNum::_4, PinAF(AF_LTDC)},	// B7
	{GPIO::F, PinNum::_13, PinAF(AF_LTDC)}, // R2
	{GPIO::G, PinNum::_15, PinAF(AF_LTDC)}, // B2
};

// SYSCFG DISPLAYCLKCR.PIXEL_CLK_SEL -- LTDC pixel-clock source (RM0457):
//   0b00 = DSI PLL clock, 0b01 = LVDS PLL clock, 0b10 = ck_ker_ltdc (flexgen 27)
constexpr uint32_t PixelClkSel_KerLtdc = 2;
constexpr unsigned FlexgenLtdc = 27;
} // namespace

void display_pins_setup()
{
	for (auto &p : ltdc_pins)
		Pin{p, PinMode::Alt, PinPull::None, PinPolarity::Normal, PinSpeed::VeryHigh};
}

// RIF: expose the LTDC register ports to our (secure) context and make the
// LTDC's DMA reads secure so they pass the RISAF DDR firewall.
//   RISUP 80 = LTDC common, 119 = LTDC layers;  RIMU 11 = LTDC_L1/L2 master
void display_rif_setup()
{
	RISC->SECCFGR[80 / 32] |= (1u << (80 % 32));
	RISC->SECCFGR[119 / 32] |= (1u << (119 % 32));
	auto attr = RIMC->ATTR[11];
	RIMC->ATTR[11] = (attr & ~RIMC_ATTR_CIDSEL) | RIMC_ATTR_MSEC | RIMC_ATTR_MPRIV;
}

uint32_t display_clocks_setup()
{
	// ck_ker_ltdc (flexgen output 27) = the LTDC pixel clock
	// Prefer PLL4 (its ~1.2 GHz gives ~1 MHz steps through the 6-bit findiv).
	// Divide down to the largest value <= Panel::MaxPixelClockHz.
	// Fall back to HSI/2 = 32 MHz if PLL4 isn't running.
	using namespace RCC_Clocks;
	uint32_t pix;
	auto s4 = get_pll_settings<PLL4>();
	uint32_t ref = s4.src == MuxSelSource::hse ? 40'000'000 : s4.src == MuxSelSource::hsi ? 64'000'000 : 0;
	uint32_t pll4 = ref ? s4.calc_freq(ref) : 0;
	if (pll4 >= Panel::MaxPixelClockHz) {
		uint32_t findiv = (pll4 + Panel::MaxPixelClockHz - 1) / Panel::MaxPixelClockHz; // ceil
		if (findiv > 64)
			findiv = 64;
		FlexbarConf{.PLL = FlexbarConf::PLLx::_4, .findiv = static_cast<uint8_t>(findiv - 1), .prediv = 0}.init(
			FlexgenLtdc);
		pix = pll4 / findiv;
	} else {
		FlexbarConf{.PLL = FlexbarConf::PLLx::HSI, .findiv = 1, .prediv = 0}.init(FlexgenLtdc); // 64/2
		pix = 32'000'000;
	}

	// bus/kernel clock gate (single gate per IP on MP25, bit 1) + reset pulse
	RCC->LTDCCFGR |= 1u << 1;
	RCC->LTDCCFGR |= 1u << 0;
	udelay(20);
	RCC->LTDCCFGR &= ~(1u << 0);

	// LTDC pixel clock source = ck_ker_ltdc (SYSCFG mux 0b10)
	SYSCFG->DISPLAYCLKCR = PixelClkSel_KerLtdc;
	return pix;
}

bool display_init(uint32_t first_fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg_argb)
{
	display_pins_setup();
	display_rif_setup();
	display_clocks_setup();
	nv3052c_init();
	ltdc_init(first_fb, x, y, w, h, bg_argb);
	return true;
}
