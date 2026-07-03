/*
 * typec_ev1.cc — Type-C port control for the STM32MP257 EV1 kit
 *
 * The TCPP03-M20 gates 5V onto VBUS in host mode. In device mode nothing
 * needs to be done: the TCPP03's dead-battery Rd pull-downs present Rd on
 * CC, so the far-end host supplies VBUS.
 */

#include "typec.hh"
#include "stm32mp2xx.h"
#include "tcpp03.hh"

static Tcpp03Controller tcpp03;

int TypeC::init()
{
	return tcpp03.init();
}

int TypeC::enter_host_mode()
{
	return tcpp03.enable_vbus();
}

int TypeC::exit_host_mode()
{
	return tcpp03.disable_vbus();
}

int TypeC::enter_device_mode()
{
	return 0;
}

int TypeC::exit_device_mode()
{
	return 0;
}

bool TypeC::vbus_present()
{
	// No VBUS sense on this board; report the forced "VBUS valid" bit so
	// device mode behaves as if a host is always attached (as before).
	return (SYSCFG->USB2PHY2CR & SYSCFG_USB2PHY2CR_VBUSVLDEXT) != 0;
}

void TypeC::log_status_changes()
{
	// CC is behind the TCPP03 and there is no VBUS sense: nothing to report
}
