#include "cdc_acm.h"
#include "drivers/pwr_vdd.hh"
#include "dwc3/core.h"
#include "dwc3/gadget.h"
#include "dwc3_baremetal.h"
#include "print/print.hh"
#include "stm32mp2xx_hal.h"
#include "xhci_baremetal.h"

/* Set to 1 for host mode, 0 for device (CDC-ACM) mode */
#define USB_HOST_MODE 1

/* ---- Device mode (CDC-ACM echo) ---- */
static void run_device_mode()
{
	struct dwc3 *dwc = dwc3_baremetal_init(DWC3_DR_MODE_PERIPHERAL, nullptr);
	if (!dwc) {
		print("DWC3 init failed\n");
		while (true)
			;
	}

	// Assert VBUS after core+gadget init but before gadget_start.
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

/* ---- Host mode ---- */
static void run_host_mode()
{
	struct dwc3 *dwc = dwc3_baremetal_init(DWC3_DR_MODE_HOST, nullptr);
	if (!dwc) {
		print("DWC3 host init failed\n");
		while (true)
			;
	}

	int ret = xhci_host_init(USB3DRD_BASE);
	if (ret) {
		printf("xHCI init failed: %d\n", ret);
		while (true)
			;
	}

	print("USB Host ready. Plug in a device.\n");

	while (true) {
		__asm__ volatile("wfe");
	}
}

int main()
{
	print("USB DRD Example\n");
	HAL_Init();
	PowerControl::enable_usb33(PowerControl::Present::If);

#if USB_HOST_MODE
	run_host_mode();
#else
	run_device_mode();
#endif
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
