#include "cdc_acm.h"
#include "drivers/pin.hh"
#include "drivers/pwr_vdd.hh"
#include "drivers/rcc.hh"
#include "drivers/rcc_xbar.hh"
#include "dwc3/core.h"
#include "dwc3/gadget.h"
#include "dwc3_baremetal.h"
#include "interrupt/interrupt.hh"
#include "print/print.hh"
#include "stm32mp2xx_hal.h"
#include <cmath>
#include <linux/delay.h>

int main()
{
	print("USB DRD Example\n");
	HAL_Init();
	PowerControl::enable_usb33(PowerControl::Present::If);

	struct dwc3 *dwc = dwc3_baremetal_init(nullptr);
	if (!dwc) {
		print("DWC3 init failed\n");
		return -1;
	}

	// Assert VBUS before gadget start so DWC3 sees VBUS during device-mode init.
	SYSCFG->USB2PHY2CR |= SYSCFG_USB2PHY2CR_VBUSVLDEXT;

	int r = cdc_acm_init(&dwc->gadget);
	print("cdc_acm_init: ", r, "\n");

	// bool conn_detected = false;

	while (true) {
		// if (vbus_det.is_on()) {
		// 	if (!conn_detected) {
		// 		conn_detected = true;
		// 		SYSCFG->USB2PHY2CR |= SYSCFG_USB2PHY2CR_VBUSVLDEXT;
		// 		printf("Connected\n");
		// 	}
		// } else {
		// 	if (conn_detected) {
		// 		conn_detected = false;
		// 		SYSCFG->USB2PHY2CR &= ~SYSCFG_USB2PHY2CR_VBUSVLDEXT;
		// 		printf("Disconnected\n");
		// 	}
		// }

		dwc3_gadget_uboot_handle_interrupt(dwc);

		char buf[64];
		int n = cdc_acm_read(buf, sizeof(buf));
		if (n > 0)
			cdc_acm_write(buf, n); // echo back
	}
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
