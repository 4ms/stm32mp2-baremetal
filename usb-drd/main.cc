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

	// Assert VBUS after core+gadget init but before gadget_start.
	// Matches U-Boot femtoPHY timing: phy_set_mode_ext sets VBUSVLDEXT=1
	// when device role is activated, which happens between dwc3_init and gadget_start.
	SYSCFG->USB2PHY2CR |= SYSCFG_USB2PHY2CR_VBUSVLDEXT;

	int r = cdc_acm_init(&dwc->gadget);
	print("cdc_acm_init: ", r, "\n");

	// Post-gadget-start register check (compare with U-Boot during ums operation)
	// U-Boot: DCFG=0x00480830 DCTL=0x8c000a00 DEVTEN=0x0000121f DSTS=0x00823a30
	printf("Post-init DCFG:  0x%08x\n", *(volatile uint32_t *)(0x4830C700));
	printf("Post-init DCTL:  0x%08x\n", *(volatile uint32_t *)(0x4830C704));
	printf("Post-init DEVTEN:0x%08x\n", *(volatile uint32_t *)(0x4830C708));
	printf("Post-init DSTS:  0x%08x\n", *(volatile uint32_t *)(0x4830C70C));

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
