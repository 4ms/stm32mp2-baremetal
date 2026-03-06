/*
 * dwc3_baremetal.c — STM32MP257 bare-metal DWC3 initialization
 *
 * Enables clocks, deasserts resets, then calls into the U-Boot DWC3 driver.
 */

#include "dwc3_baremetal.h"
#include <common.h>
#include <dwc3-uboot.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>

/* STM32MP257 USB3DRD is on AHB5 at offset 0x100000 */
#define USB3DRD_BASE_ADDR	0x48300000UL

/* RCC register offsets for OTG and USB2PHY2 */
#define RCC_BASE_ADDR		0x44200000UL
#define RCC_OTGCFGR		(*(volatile uint32_t *)(RCC_BASE_ADDR + 0x0808UL))
#define RCC_USB2PHY2CFGR	(*(volatile uint32_t *)(RCC_BASE_ADDR + 0x080CUL))

/* Bit definitions */
#define RCC_OTGCFGR_OTGRST	(1U << 0)
#define RCC_OTGCFGR_OTGEN	(1U << 1)
#define RCC_USB2PHY2CFGR_RST	(1U << 0)
#define RCC_USB2PHY2CFGR_EN	(1U << 1)

struct dwc3 *dwc3_baremetal_init(const dwc3_platform_t *platform)
{
	/* 1. Enable USB2 PHY2 clock and deassert reset */
	RCC_USB2PHY2CFGR |= RCC_USB2PHY2CFGR_EN;
	RCC_USB2PHY2CFGR &= ~RCC_USB2PHY2CFGR_RST;
	udelay(100);

	/* 2. Enable OTG clock and deassert reset */
	RCC_OTGCFGR |= RCC_OTGCFGR_OTGEN;
	RCC_OTGCFGR &= ~RCC_OTGCFGR_OTGRST;
	udelay(100);

	/* 3. Populate dwc3_device for the U-Boot driver */
	struct dwc3_device dwc3_dev;
	memset(&dwc3_dev, 0, sizeof(dwc3_dev));

	dwc3_dev.base = platform ? platform->regs_base : USB3DRD_BASE_ADDR;
	dwc3_dev.dr_mode = USB_DR_MODE_PERIPHERAL;
	dwc3_dev.maximum_speed = USB_SPEED_HIGH;
	dwc3_dev.index = 0;

	if (platform) {
		dwc3_dev.dis_u2_susphy_quirk = platform->dis_u2_susphy_quirk;
		dwc3_dev.dis_enblslpm_quirk = platform->dis_enblslpm_quirk;
	}

	/* 4. Call into U-Boot DWC3 core init */
	int ret = dwc3_uboot_init(&dwc3_dev);
	if (ret) {
		printf("dwc3_uboot_init failed: %d\n", ret);
		return NULL;
	}

	/* 5. Retrieve the dwc3 pointer */
	struct dwc3 *dwc = dwc3_uboot_get(dwc3_dev.index);
	if (!dwc) {
		printf("dwc3_uboot_get failed\n");
		return NULL;
	}

	return dwc;
}

void dwc3_baremetal_shutdown(struct dwc3 *dwc)
{
	dwc3_uboot_exit(0);

	/* Disable clocks */
	RCC_OTGCFGR &= ~RCC_OTGCFGR_OTGEN;
	RCC_USB2PHY2CFGR &= ~RCC_USB2PHY2CFGR_EN;
}
