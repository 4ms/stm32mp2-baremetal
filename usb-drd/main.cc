#include "cdc_acm.h"
#include "drivers/pin.hh"
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

	struct dwc3 *dwc = dwc3_baremetal_init(nullptr);
	if (!dwc) {
		print("DWC3 init failed\n");
		return -1;
	}

	cdc_acm_init(&dwc->gadget);

	while (true) {
		dwc3_gadget_uboot_handle_interrupt(dwc);

		char buf[64];
		int n = cdc_acm_read(buf, sizeof(buf));
		if (n > 0)
			cdc_acm_write(buf, n); // echo back

		asm("nop");
	}
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
