#include "cdc_acm.h"
#include "drivers/pwr_vdd.hh"
#include "dwc3/core.h"
#include "dwc3/gadget.h"
#include "dwc3_baremetal.h"
#include "print/print.hh"
#include "stm32mp2xx_hal.h"
#include "usb_midi.h"
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

/* MIDI status byte to human-readable name */
static const char *midi_status_name(uint8_t status)
{
	switch (status & 0xF0) {
	case 0x80: return "NoteOff";
	case 0x90: return "NoteOn ";
	case 0xA0: return "AfterT ";
	case 0xB0: return "CC     ";
	case 0xC0: return "PrgChg ";
	case 0xD0: return "ChanPr ";
	case 0xE0: return "PBend  ";
	case 0xF0: return "System ";
	default:   return "???    ";
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

	print("USB Host ready. Plug in a MIDI device.\n");

	while (true) {
		struct usb_device *dev = xhci_host_poll();
		if (dev) {
			/* New device — try to bind as MIDI */
			ret = usb_midi_init(dev);
			if (ret) {
				printf("Not a MIDI device (or init failed: %d)\n", ret);
				continue;
			}

			printf("\nListening for MIDI...\n\n");

			/* Read MIDI events */
			while (usb_midi_is_connected()) {
				struct usb_midi_event events[16];
				int n = usb_midi_read(events, 16);
				if (n < 0) {
					printf("MIDI read error: %d\n", n);
					break;
				}
				if (n == 0)
					continue;
				for (int i = 0; i < n; i++) {
					/* Skip empty/padding packets */
					if (events[i].header == 0)
						continue;
					uint8_t cin = events[i].header & 0x0F;
					uint8_t cable = events[i].header >> 4;
					printf("MIDI [%d] %s ch%-2d  %02x %02x  (CIN=%x)\n",
					       cable,
					       midi_status_name(events[i].midi[0]),
					       (events[i].midi[0] & 0x0F) + 1,
					       events[i].midi[1],
					       events[i].midi[2],
					       cin);
				}
			}
		}
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
