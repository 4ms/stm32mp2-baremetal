/*
 * xhci_baremetal.h — Bare-metal xHCI host API
 */

#ifndef XHCI_BAREMETAL_H
#define XHCI_BAREMETAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct xhci_ctrl;
struct usb_device;

/*
 * xhci_host_init - Initialize xHCI host controller
 *
 * Call after dwc3_baremetal_init(DWC3_DR_MODE_HOST, ...) has configured
 * the DWC3 in host mode. Powers on ports and allocates the root hub.
 *
 * @dwc3_base: DWC3 MMIO base address (e.g. 0x48300000)
 * Returns 0 on success, negative on error.
 */
int xhci_host_init(uintptr_t dwc3_base);

/*
 * xhci_host_poll - Poll for port changes and enumerate new devices
 *
 * Call periodically from main loop. On new device connection,
 * resets the port, assigns an address, reads descriptors, and
 * sets configuration.
 *
 * Returns pointer to newly enumerated usb_device, or NULL if no change.
 */
struct usb_device *xhci_host_poll(void);

/*
 * xhci_device_connected - Check if a device is currently attached
 */
bool xhci_device_connected(void);

/*
 * xhci_host_get_ctrl - Get the xHCI controller instance
 */
struct xhci_ctrl *xhci_host_get_ctrl(void);

/*
 * xhci_host_shutdown - Stop and clean up xHCI
 */
void xhci_host_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* XHCI_BAREMETAL_H */
