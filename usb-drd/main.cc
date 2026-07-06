#include "cdc_acm.h"
#include "drivers/button.hh"
#include "drivers/pwr_vdd.hh"
#include "dwc3/core.h"
#include "dwc3/gadget.h"
#include "dwc3_baremetal.h"
#include "midi_status.hh"
#include "print/print.hh"
#include "stm32mp2xx_hal.h"
#include "typec.hh"
#include "usb_midi.h"
#include "usb_msc.h"
#include "xhci_baremetal.h"

enum class UsbMode { Host, Device };

static struct dwc3 *dwc;

/*
 * Check if User2 button was pressed (with simple debounce).
 * Returns true on rising edge only, at most once per 300ms.
 */
static bool button_pressed()
{
	static bool was_pressed = false;
	static uint32_t last_edge_ms = 0;

	bool pressed = button_user2_pressed();
	if (pressed && !was_pressed && (HAL_GetTick() - last_edge_ms) > 300) {
		last_edge_ms = HAL_GetTick();
		was_pressed = true;
		return true;
	}
	was_pressed = pressed;
	return false;
}

/* ---- Host mode ---- */

static bool host_init()
{
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

	TypeC::enter_host_mode();

	print("USB Host ready. Plug in a device.\n");
	return true;
}

static void host_shutdown()
{
	usb_midi_disconnect();
	usb_msc_disconnect();
	xhci_host_shutdown();
	TypeC::exit_host_mode();
	dwc3_baremetal_shutdown(dwc);
	dwc = nullptr;
	print("Host mode stopped.\n");
}

/* Print `len` bytes of `data` as an offset-prefixed hex+ASCII dump. */
static void hexdump(const uint8_t *data, int len)
{
	for (int i = 0; i < len; i += 16) {
		printf("    %04x  ", i);
		for (int j = 0; j < 16; j++) {
			if (i + j < len)
				printf("%02x ", data[i + j]);
			else
				printf("   ");
		}
		printf(" ");
		for (int j = 0; j < 16 && i + j < len; j++) {
			uint8_t c = data[i + j];
			printf("%c", (c >= 0x20 && c < 0x7f) ? c : '.');
		}
		printf("\n");
	}
}

/* Read MIDI events until the device disconnects or a mode switch is attempted. */
static void run_midi_device()
{
	printf("\nListening for MIDI...\n\n");

	while (xhci_device_connected()) {
		if (button_pressed())
			printf("Cannot switch mode: device attached\n");

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
	printf("MIDI device removed, waiting for new device...\n");
}

/*
 * Identify a mass-storage device (INQUIRY + READ CAPACITY), dump the first
 * block, then wait for it to be unplugged. Read-only — nothing is written.
 */
static void run_msc_device()
{
	char vendor[9], product[17];
	uint8_t type = 0;
	if (usb_msc_inquiry(vendor, product, &type) == 0)
		printf("  Vendor: '%s'  Product: '%s'  (SCSI type %d)\n",
			   vendor, product, type);

	/* Drives often report not-ready / unit-attention on first access. */
	int ready = -1;
	for (int i = 0; i < 20 && xhci_device_connected(); i++) {
		ready = usb_msc_test_unit_ready();
		if (ready == 0)
			break;
		HAL_Delay(50);
	}
	if (ready != 0) {
		printf("  Unit not ready; skipping read\n");
	} else {
		uint32_t last_lba = 0, bs = 0;
		if (usb_msc_read_capacity(&last_lba, &bs) == 0 && bs) {
			uint64_t total = (uint64_t)(last_lba + 1) * bs;
			printf("  Capacity: %lu blocks x %lu bytes (%lu MiB)\n",
				   (unsigned long)(last_lba + 1), (unsigned long)bs,
				   (unsigned long)(total >> 20));

			static uint8_t block[4096] __attribute__((aligned(64)));
			if (bs <= sizeof(block) &&
				usb_msc_read_blocks(0, 1, block) == 0) {
				printf("  First block (LBA 0), first 64 bytes:\n");
				hexdump(block, 64);
				if (block[510] == 0x55 && block[511] == 0xAA)
					printf("  MBR boot signature present (0x55AA)\n");
			} else {
				printf("  Block read failed\n");
			}
		} else {
			printf("  READ CAPACITY failed\n");
		}
	}

	printf("Storage ready. Unplug to remove.\n");
	while (xhci_device_connected()) {
		if (button_pressed())
			printf("Cannot switch mode: device attached\n");
		xhci_host_poll();
	}
	printf("Mass storage device removed, waiting for new device...\n");
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

		/* New device — try each supported class in turn */
		if (usb_midi_init(dev) == 0) {
			run_midi_device();
			usb_midi_disconnect();
		} else if (usb_msc_init(dev) == 0) {
			printf("\nMass Storage device attached:\n");
			run_msc_device();
			usb_msc_disconnect();
		} else {
			printf("Unsupported device (not MIDI or Mass Storage)\n");
		}
	}
}

/* ---- Device mode (CDC-ACM echo) ---- */

static bool device_init()
{
	TypeC::enter_device_mode();

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
	TypeC::exit_device_mode();
	print("Device mode stopped.\n");
}

/*
 * Run device mode until a mode switch is requested.
 * Returns true if the user requested a mode toggle.
 */
static const char *usb_speed_name(int speed)
{
	switch (speed) {
	case USB_SPEED_LOW:	return "Low";
	case USB_SPEED_FULL:	return "Full";
	case USB_SPEED_HIGH:	return "High";
	case USB_SPEED_SUPER:	return "Super";
	default:		return "no link";
	}
}

static bool device_run()
{
	bool host_connected = false;
	int last_speed = USB_SPEED_UNKNOWN;
	bool was_configured = false;

	while (true) {
		dwc3_gadget_uboot_handle_interrupt(dwc);

		/* Report state changes: CC/VBUS, link, configuration */
		TypeC::log_status_changes();
		if ((int)dwc->gadget.speed != last_speed) {
			last_speed = dwc->gadget.speed;
			printf("USB: link: %s\n", usb_speed_name(last_speed));
		}
		if (cdc_acm_is_configured() != was_configured) {
			was_configured = !was_configured;
			printf("USB: %s by host\n",
				   was_configured ? "configured" : "unconfigured");
		}

		if (button_pressed()) {
			if (host_connected) {
				printf("Cannot switch mode: host attached\n");
			} else {
				return true;
			}
		}

		/* Detect host connection via VBUS or USB state */
		bool vbus_now = TypeC::vbus_present();
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
	TypeC::init();

	UsbMode mode = UsbMode::Device;

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
