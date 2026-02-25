#include "drivers/pwr_vdd.hh"
#include "drivers/rcc.hh"
#include "drivers/rcc_pll.hh"
#include <cstdint>

extern "C" {
uint32_t SystemCoreClock = 1'500'000'000;
}

void system_init()
{
	// #if (RUN_EL3)

	constexpr RCC_Clocks::PLLSettings pll7{
		.src = RCC_Clocks::MuxSelSource::hse,
		.mult = 167,
		.refdiv = 4,
		.postdiv1 = 1,
		.postdiv2 = 2,
		.frac = 0x1DE69B,
	};
	// suitable for 48kHz audio: 48kHz * 256 * 68 = 835'584'000
	static_assert(pll7.calc_freq(40'000'000) >= 835'584'000);
	static_assert(pll7.calc_freq(40'000'000) <= 835'584'100);

	RCC_Clocks::set_pll<RCC_Clocks::PLL7>(pll7);

	// VDDIO_SDCARD from PMIC LDO8. Marked "regulator-always-on" in TF-A BL2 dts
	PowerControl::enable_vddio1(PowerControl::Present::Always);

	// 1V8 from PMIC Buck5. Marked "regulator-always-on" in TF-A BL2 dts
	PowerControl::enable_vddio2(PowerControl::Present::Always);

	// VDDIO from PMIC Buck4. Marked "regulator-always-on" in TF-A BL2 dts
	PowerControl::enable_vddio3(PowerControl::Present::Always);
	PowerControl::enable_vddio4(PowerControl::Present::Always);
	// #endif
}
