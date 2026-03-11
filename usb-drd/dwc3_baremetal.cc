/*
 * dwc3_baremetal.c — STM32MP257 bare-metal DWC3 initialization
 *
 * Enables clocks, deasserts resets, then calls into the U-Boot DWC3 driver.
 */

#include "dwc3_baremetal.h"
#include "drivers/rcc_pll.hh"
#include "drivers/rcc_xbar.hh"
#include "dwc3/core.h"
#include "dwc3/io.h"
#include "stm32mp2xx.h"
#include <common.h>
#include <dwc3-uboot.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>

/* STM32MP257 USB3DRD is on AHB5 at offset 0x100000 */
#define USB3DRD_BASE_ADDR 0x48300000UL

/* SYSCFG USB3DRCR — USB3DR controller wrapper configuration */
#define SYSCFG_BASE_ADDR 0x44230000UL
#define SYSCFG_USB3DRCR (*(volatile uint32_t *)(SYSCFG_BASE_ADDR + 0x4800UL))
#define USB3DRCR_USB2ONLYD (1U << 4) /* USB2-only device mode */

struct dwc3 *dwc3_baremetal_init(const dwc3_platform_t *platform)
{
	// Open RISAF4 region for DWC3 DMA access to DDR at 0x8A000000.
	// Without this, the USB3DRD bus master's DMA writes are silently blocked.
	if (!(RISAF4->CR & RISAF_CR_GLOCK)) {
		auto &reg = RISAF4->REG[14];
		reg.CFGR &= ~RISAF_REGCFGR_BREN;
		reg.STARTR = 0x0A000000;
		reg.ENDR = 0x0A1FFFFF;
		reg.CIDCFGR = RISAF_REGCIDCFGR_RDEN | RISAF_REGCIDCFGR_WREN;
		reg.CFGR = RISAF_REGCFGR_BREN;
	}

	// Enable clocks before reset??

	// Enable USB2 PHY2 clock
	RCC->USB2PHY2CFGR |= RCC_USB2PHY2CFGR_USB2PHY2EN;
	udelay(100);

	// Enable USB3DR clock
	RCC->USB3DRDCFGR |= RCC_USB3DRDCFGR_USB3DRDEN;
	udelay(100);

	RCC->USB3PCIEPHYCFGR |= RCC_USB3PCIEPHYCFGR_USB3PCIEPHYEN;

	// Keep PHY analog blocks powered during Suspend/Sleep (match U-Boot femtophy device mode).
	// When USB2PHY2CMN=1, the PHY powers down its PLL/analog blocks on suspend, stopping the
	// UTMI clock to the DWC3 and preventing endpoint event generation.
	SYSCFG->USB2PHY2CR &= ~SYSCFG_USB2PHY2CR_USB2PHY2CMN;

	////////////////////
	// From RM0457: Sec 83.3.2:
	// 1. Assert rstn_usb2phy2, rstn_usb3dr and rstn_usb3pciephy.
	RCC->USB2PHY2CFGR |= RCC_USB2PHY2CFGR_USB2PHY2RST;
	RCC->USB3DRDCFGR |= RCC_USB3DRDCFGR_USB3DRDRST;
	RCC->USB3PCIEPHYCFGR |= RCC_USB3PCIEPHYCFGR_USB3PCIEPHYRST;

	// 2. Select the frequency of USB2PHY2 reference clock.
	FlexbarConf{.PLL = FlexbarConf::PLLx::_4, .findiv = 30, .prediv = 2}.init(58);
	RCC->USB2PHY2CFGR &= ~RCC_USB2PHY2CFGR_USB2PHY2CKREFSEL; // 1: HSE, 0: flexgen ch. 58
	// 0b001: 20MHz, 0b000: 19.2MHZ, 0b010: 24MHz
	SYSCFG->USB2PHY2CR =
		(SYSCFG->USB2PHY2CR & ~SYSCFG_USB2PHY2CR_USB2PHY2SEL_Msk) | (0b001 << SYSCFG_USB2PHY2CR_USB2PHY2SEL_Pos);

	// 3. Program the USB2PHY2 trimming bits (if needed).
	// nop: skip

	// 4. Enable the clocks coming from RCC or oscillator.
	// nop: PLL4 is already enabled and running

	// 5. Select USB3 or PCIe mode.
	// 0 = COMBOPHY used for PCI , 1 = COMBOPHY used for USB3
	SYSCFG->COMBOPHYCR2 &= ~SYSCFG_COMBOPHYCR2_COMBOPHY_MODESEL;
	// Configure USB3DR for USB2-only device mode.
	SYSCFG->USB3DRCR |= SYSCFG_USB3DRCR_USB3DR_USB2ONLYD;

	// 6. Select the reference clock path and frequency, and program the PLL for COMBOPHY.
	// 7. Enable the reference clock buffer in the COMBOPHY.
	// nop: not using COMBOPHY

	// 8. Wait for 10 μs.
	udelay(10);

	// 9. Release rstn_usb2phy2 and rstn_usb3pciephy.
	RCC->USB2PHY2CFGR &= ~RCC_USB2PHY2CFGR_USB2PHY2RST;
	RCC->USB3PCIEPHYCFGR &= ~RCC_USB3PCIEPHYCFGR_USB3PCIEPHYRST;

	// 10. Wait for 260 μs (COMBOPHY PLL lock time and start of generation of pipe0_pclk, this
	// wait time also ensures that USB2PHY2 PLL is locked, and FREECLK is generated).
	udelay(260);

	// 11. Release rst_n_usb3dr.
	RCC->USB3DRDCFGR &= ~RCC_USB3DRDCFGR_USB3DRDRST;

	////////////////////

	// Enable VBUS valid comparator
	// Doesn't work (no hard-wired VBUS Detect pin)
	// SYSCFG->USB2PHY2CR &= ~SYSCFG_USB2PHY2CR_OTGDISABLE0;
	// Use external comparator in device mode (init to no vbus)
	SYSCFG->USB2PHY2CR &= ~SYSCFG_USB2PHY2CR_VBUSVLDEXT;
	SYSCFG->USB2PHY2CR |= SYSCFG_USB2PHY2CR_VBUSVLDEXTSEL;

	// Select PHYIF? 0: 8 bits 60MHZ, 1: 16 bits 30MHz
	// USB3->GBLREGS.GUSB2PHYCFG |= USB3_GUSB2PHYCFG_PHYIF;

	/* Populate dwc3_device for the U-Boot driver */
	struct dwc3_device dwc3_dev;
	memset(&dwc3_dev, 0, sizeof(dwc3_dev));

	dwc3_dev.base = platform ? platform->regs_base : USB3DRD_BASE_ADDR;
	dwc3_dev.dr_mode = USB_DR_MODE_PERIPHERAL;
	dwc3_dev.maximum_speed = USB_SPEED_HIGH;
	dwc3_dev.index = 0;

	if (platform) {
		dwc3_dev.dis_enblslpm_quirk = platform->dis_enblslpm_quirk;
	}

	/* Without a PHY driver managing suspend/resume, SUSPHY must be
	 * cleared or the USB2 PHY clock stops and endpoint commands hang. */
	dwc3_dev.dis_u2_susphy_quirk = 1;
	dwc3_dev.dis_u3_susphy_quirk = 1;

	/* Call into U-Boot DWC3 core init */
	int ret = dwc3_uboot_init(&dwc3_dev);
	if (ret) {
		printf("dwc3_uboot_init failed: %d\n", ret);
		return NULL;
	}

	/* Retrieve the dwc3 pointer */
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
	RCC->USB2PHY2CFGR &= ~RCC_USB2PHY2CFGR_USB2PHY2EN;
	RCC->USB3DRDCFGR &= ~RCC_USB3DRDCFGR_USB3DRDEN;
}
