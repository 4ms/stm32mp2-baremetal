#include "cdc_acm.h"
#include "drivers/pwr_vdd.hh"
#include "dwc3/core.h"
#include "dwc3/gadget.h"
#include "dwc3_baremetal.h"
#include "print/print.hh"
#include "stm32mp2xx_hal.h"

int main()
{
	print("USB DRD Example\n");
	HAL_Init();
	PowerControl::enable_usb33(PowerControl::Present::If);

	struct dwc3 *dwc = dwc3_baremetal_init(nullptr);
	if (!dwc) {
		print("DWC3 init failed\n");
		while (true)
			;
	}

	// Assert VBUS after core+gadget init but before gadget_start.
	// Matches U-Boot femtoPHY timing: phy_set_mode_ext sets VBUSVLDEXT=1
	// when device role is activated, which happens between dwc3_init and gadget_start.
	SYSCFG->USB2PHY2CR |= SYSCFG_USB2PHY2CR_VBUSVLDEXT;

	int r = cdc_acm_init(&dwc->gadget);
	if (r) {
		printf("cdc_acm_init failed: %d\n", r);
		while (true)
			;
	}

	while (true) {
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
