/*
 * typec_devboard.cc — Type-C port control for the custom dev board (v0.1)
 *
 * The on-board USB-C jack's CC1/CC2 pins are wired directly to the
 * STM32MP257's USBPD_CC1/USBPD_CC2 pins, so the built-in UCPD1 peripheral
 * presents the Type-C terminations (no external port controller):
 *   - Device mode: UCPD1 presents Rd on both CC pins, so the far-end host
 *     supplies VBUS.
 *   - Host mode: UCPD1 presents default-USB Rp on both CC pins. The board
 *     itself cannot source VBUS; when the adaptor board is attached, PB4
 *     closes its VBUS switch (which feeds both jacks' VBUS pins).
 *
 * VBUS is sensed on PH3 through a 430k/620k divider (5V -> 2.95V), which
 * reads as a valid logic high.
 *
 * No USB-PD messaging is done — only the analog Rd/Rp terminations and the
 * CC voltage comparators are used, so the CC pins just need VDD33UCPD
 * powered and a kernel clock for the detector logic.
 */

#include "typec.hh"
#include "drivers/pin.hh"
#include "drivers/rcc_xbar.hh"
#include "stm32mp2xx_hal.h"
#include <cstdio>

namespace
{

Pin vbus_enable; // PB4: closes the adaptor board's VBUS switch
Pin vbus_sense;	 // PH3: VBUS through 430k/620k divider

bool wait_pwr_ready(uint32_t rdy_mask, unsigned timeout_ms)
{
	while (!(PWR->CR1 & rdy_mask)) {
		if (timeout_ms-- == 0)
			return false;
		HAL_Delay(1);
	}
	return true;
}

// UCPD_SR.TYPEC_VSTATE_CCx, per RM0457:
// as sink (ANAMODE=1): 0 = open/vRa, 1..3 = source Rp detected (default/1.5A/3A)
// as source (ANAMODE=0): 0 = vRa, 1 = vRd (sink attached), 2 = open
void print_cc_state(const char *when)
{
	uint32_t sr = UCPD1->SR;
	unsigned cc1 = (sr & UCPD_SR_TYPEC_VSTATE_CC1_Msk) >> UCPD_SR_TYPEC_VSTATE_CC1_Pos;
	unsigned cc2 = (sr & UCPD_SR_TYPEC_VSTATE_CC2_Msk) >> UCPD_SR_TYPEC_VSTATE_CC2_Pos;
	printf("TypeC: %s: CC1 vstate=%u, CC2 vstate=%u, VBUS %s\n",
		   when,
		   cc1,
		   cc2,
		   TypeC::vbus_present() ? "present" : "absent");
}

} // namespace

int TypeC::init()
{
	// CC1/CC2 are powered from VDD33UCPD: remove the PWR isolation.
	// (If this warns, check that VDD33UCPD is connected to 3.3V.)
	PWR->CR1 |= PWR_CR1_UCPDVMEN;
	if (!wait_pwr_ready(PWR_CR1_UCPDRDY, 10))
		printf("TypeC: WARNING: VDD33UCPD not detected, CC pins will not work\n");
	PWR->CR1 &= ~PWR_CR1_UCPDVMEN;
	PWR->CR1 |= PWR_CR1_UCPDSV;

	// PB4 (VBUS switch enable) is powered from VDDIO4: remove that isolation too.
	PWR->CR1 |= PWR_CR1_VDDIO4VMEN;
	if (!wait_pwr_ready(PWR_CR1_VDDIO4RDY, 10))
		printf("TypeC: WARNING: VDDIO4 not detected, PB4 (VBUS enable) will not work\n");
	PWR->CR1 &= ~PWR_CR1_VDDIO4VMEN;
	PWR->CR1 |= PWR_CR1_VDDIO4SV;

	// ucpd_ker_ck = HSI/4 = 16MHz (flexgen channel 35). Only clocks the
	// Type-C detector/debounce logic, so the exact rate is not critical.
	FlexbarConf ucpd_clk{.PLL = FlexbarConf::PLLx::HSI, .findiv = 3, .prediv = 0};
	ucpd_clk.init(35);
	__HAL_RCC_UCPD1_CLK_ENABLE();

	UCPD1->CFG1 = UCPD_CFG1_UCPDEN;
	UCPD1->CR = 0; // no terminations until a mode is entered

	vbus_enable.init({GPIO::B, PinNum::_4, AFNone}, PinMode::Output);
	vbus_enable.off();
	vbus_sense.init({GPIO::H, PinNum::_3, AFNone}, PinMode::Input);

	printf("TypeC: UCPD1 init (direct CC, VBUS switch on PB4)\n");
	return 0;
}

int TypeC::enter_host_mode()
{
	// Source: present default-USB Rp on both CC pins
	UCPD1->CR = UCPD_CR_ANASUBMODE_0 | UCPD_CR_CCENABLE;

	// 5V onto VBUS — only has an effect when the adaptor board is attached
	vbus_enable.on();

	HAL_Delay(100); // let CC and VBUS settle before reporting
	print_cc_state("host mode (Rp)");
	return 0;
}

int TypeC::exit_host_mode()
{
	vbus_enable.off();
	UCPD1->CR = 0;
	return 0;
}

int TypeC::enter_device_mode()
{
	// Sink: present Rd on both CC pins so the host supplies VBUS
	UCPD1->CR = UCPD_CR_ANAMODE | UCPD_CR_CCENABLE;

	HAL_Delay(100);
	print_cc_state("device mode (Rd)");
	return 0;
}

int TypeC::exit_device_mode()
{
	UCPD1->CR = 0;
	return 0;
}

bool TypeC::vbus_present()
{
	return vbus_sense.read_raw();
}
