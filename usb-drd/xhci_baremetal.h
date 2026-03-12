/*
 * xhci_baremetal.h — Bare-metal xHCI host API
 */

#ifndef XHCI_BAREMETAL_H
#define XHCI_BAREMETAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct xhci_ctrl;

/*
 * xhci_host_init - Initialize xHCI host controller
 *
 * Call after dwc3_baremetal_init(DWC3_DR_MODE_HOST, ...) has configured
 * the DWC3 in host mode. The DWC3 base address provides access to the
 * xHCI capability/operational registers at offset 0x0.
 *
 * @dwc3_base: DWC3 MMIO base address (e.g. 0x48300000)
 * Returns 0 on success, negative on error.
 */
int xhci_host_init(uintptr_t dwc3_base);

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
