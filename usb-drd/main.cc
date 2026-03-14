#include "cdc_acm.h"
#include "drivers/button.hh"
#include "drivers/pwr_vdd.hh"
#include "dwc3/core.h"
#include "dwc3/gadget.h"
#include "dwc3_baremetal.h"
#include "print/print.hh"
#include "stm32mp2xx_hal.h"
#include "tcpp03.hh"
#include "usb_midi.h"
#include "xhci_baremetal.h"

/* MIDI status byte to human-readable name */
static const char *midi_status_name(uint8_t status)
{
	switch (status & 0xF0) {
		case 0x80:
			return "NoteOff";
		case 0x90:
			return "NoteOn ";
		case 0xA0:
			return "AfterT ";
		case 0xB0:
			return "CC     ";
		case 0xC0:
			return "PrgChg ";
		case 0xD0:
			return "ChanPr ";
		case 0xE0:
			return "PBend  ";
		case 0xF0:
			return "System ";
		default:
			return "???    ";
	}
}

enum class UsbMode { Host, Device };

static Tcpp03Controller tcpp03;
static struct dwc3 *dwc;

/*
 * Check if User2 button was pressed (with simple debounce).
 * Returns true on rising edge only.
 */
static bool button_pressed()
{
	static bool was_pressed = false;
	bool pressed = button_user2_pressed();
	if (pressed && !was_pressed) {
		was_pressed = true;
		return true;
	}
	was_pressed = pressed;
	return false;
}

/* ---- Host mode ---- */

static bool host_init()
{
	tcpp03.init();

	dwc = dwc3_baremetal_init(DWC3_DR_MODE_HOST, nullptr);
	if (!dwc) {
		print("DWC3 host init failed\n");
		return false;
	}

	int ret = xhci_host_init(USB3DRD_BASE);
	if (ret) {
		printf("xHCI init failed: %d\n", ret);
		return false;
	}

	tcpp03.enable_vbus();

	print("USB Host ready. Plug in a device.\n");
	return true;
}

static void host_shutdown()
{
	usb_midi_disconnect();
	xhci_host_shutdown();
	tcpp03.disable_vbus();
	dwc3_baremetal_shutdown(dwc);
	dwc = nullptr;
	print("Host mode stopped.\n");
}

/*
 * Run host mode until a mode switch is requested.
 * Returns true if the user requested a mode toggle.
 */
static bool host_run()
{
	while (true) {
		if (button_pressed()) {
			if (xhci_device_connected()) {
				printf("Cannot switch mode: device attached\n");
			} else {
				return true;
			}
		}

		struct usb_device *dev = xhci_host_poll();
		if (!dev)
			continue;

		/* Skip hubs — their downstream devices come later */
		if (xhci_device_is_hub(dev))
			continue;

		/* New device — try to bind as MIDI */
		int ret = usb_midi_init(dev);
		if (ret) {
			printf("Not a MIDI device (or init failed: %d)\n", ret);
			continue;
		}

		printf("\nListening for MIDI...\n\n");

		/* Read MIDI events until device disconnects */
		while (xhci_device_connected()) {
			if (button_pressed()) {
				printf("Cannot switch mode: device attached\n");
			}

			/* Poll xHCI for port changes (disconnect) */
			xhci_host_poll();
			if (!xhci_device_connected())
				break;

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

		usb_midi_disconnect();
		printf("MIDI device removed, waiting for new device...\n");
	}
}

/* ---- Device mode (CDC-ACM echo) ---- */

static bool device_init()
{
	dwc = dwc3_baremetal_init(DWC3_DR_MODE_PERIPHERAL, nullptr);
	if (!dwc) {
		print("DWC3 device init failed\n");
		return false;
	}

	/* Assert VBUS after core+gadget init but before gadget_start */
	SYSCFG->USB2PHY2CR |= SYSCFG_USB2PHY2CR_VBUSVLDEXT;

	int r = cdc_acm_init(&dwc->gadget);
	if (r) {
		printf("cdc_acm_init failed: %d\n", r);
		return false;
	}

	print("USB Device ready (CDC-ACM echo).\n");
	return true;
}

static void device_shutdown()
{
	dwc3_baremetal_shutdown(dwc);
	dwc = nullptr;
	print("Device mode stopped.\n");
}

/*
 * Run device mode until a mode switch is requested.
 * Returns true if the user requested a mode toggle.
 */
static bool device_run()
{
	bool host_connected = false;

	while (true) {
		dwc3_gadget_uboot_handle_interrupt(dwc);

		if (button_pressed()) {
			if (host_connected) {
				printf("Cannot switch mode: host attached\n");
			} else {
				return true;
			}
		}

		/* Detect host connection via VBUS or USB state */
		bool vbus_now = (SYSCFG->USB2PHY2CR & SYSCFG_USB2PHY2CR_VBUSVLDEXT) != 0;
		if (vbus_now != host_connected) {
			host_connected = vbus_now;
		}

		char buf[64];
		int n = cdc_acm_read(buf, sizeof(buf));
		if (n > 0)
			cdc_acm_write(buf, n); /* echo back */
	}
}

/* ---- Main ---- */

int main()
{
	print("USB DRD Example\n");
	HAL_Init();
	PowerControl::enable_usb33(PowerControl::Present::If);
	button_user2_init();

	UsbMode mode = UsbMode::Host;

	while (true) {
		if (mode == UsbMode::Host) {
			printf("--- Entering HOST mode (press User2 to switch) ---\n");
			if (!host_init()) {
				printf("Host init failed, retrying in device mode\n");
				mode = UsbMode::Device;
				continue;
			}
			if (host_run())
				host_shutdown();
			mode = UsbMode::Device;
		} else {
			printf("--- Entering DEVICE mode (press User2 to switch) ---\n");
			if (!device_init()) {
				printf("Device init failed, retrying in host mode\n");
				mode = UsbMode::Host;
				continue;
			}
			if (device_run())
				device_shutdown();
			mode = UsbMode::Host;
		}
	}
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
