#ifndef _DWC3_UBOOT_H
#define _DWC3_UBOOT_H

#include <linux/types.h>
#include <linux/usb/otg.h>
#include <linux/usb/phy.h>

#ifdef __cplusplus
extern "C" {
#endif

/* struct dwc3_device — initialization data passed to dwc3_uboot_init().
 * In U-Boot this is populated from device-tree; in bare-metal the
 * application fills it directly. */
struct dwc3_device {
	uintptr_t base;
	enum usb_dr_mode dr_mode;
	u32 maximum_speed;
	enum usb_phy_interface hsphy_mode;
	int index;

	unsigned tx_fifo_resize : 1;
	unsigned has_lpm_erratum : 1;
	u8 lpm_nyet_threshold;
	unsigned is_utmi_l1_suspend : 1;
	u8 hird_threshold;

	unsigned disable_scramble_quirk : 1;
	unsigned u2exit_lfps_quirk : 1;
	unsigned u2ss_inp3_quirk : 1;
	unsigned req_p1p2p3_quirk : 1;
	unsigned del_p1p2p3_quirk : 1;
	unsigned del_phy_power_chg_quirk : 1;
	unsigned lfps_filter_quirk : 1;
	unsigned rx_detect_poll_quirk : 1;
	unsigned dis_u3_susphy_quirk : 1;
	unsigned dis_u2_susphy_quirk : 1;
	unsigned dis_del_phy_power_chg_quirk : 1;
	unsigned dis_tx_ipgap_linecheck_quirk : 1;
	unsigned dis_enblslpm_quirk : 1;
	unsigned dis_u2_freeclk_exists_quirk : 1;

	unsigned tx_de_emphasis_quirk : 1;
	unsigned tx_de_emphasis : 2;
};

struct dwc3;

int dwc3_uboot_init(struct dwc3_device *dwc3_dev);
void dwc3_uboot_exit(int index);
void dwc3_uboot_handle_interrupt(int index);
struct dwc3 *dwc3_uboot_get(int index);

#ifdef __cplusplus
}
#endif

#endif /* _DWC3_UBOOT_H */
