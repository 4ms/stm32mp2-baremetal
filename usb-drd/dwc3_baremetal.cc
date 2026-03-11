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

	// Use external VBUS comparator in device mode.
	// Match U-Boot femtoPHY: select external comparator (VBUSVLDEXTSEL=1)
	// but do NOT assert VBUS yet (VBUSVLDEXT=0).  VBUS is asserted later,
	// after dwc3 core+gadget init, just before gadget_start — same timing
	// as U-Boot's phy_set_mode_ext call.
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

	/* Do NOT set dis_u2/u3_susphy_quirk: U-Boot leaves SUSPHY=1 in both
	 * GUSB2PHYCFG and GUSB3PIPECTL.  The PHY clock is kept alive by
	 * USB2PHY2CMN=0 (cleared above), so PHY suspension is harmless. */

	/* Call into U-Boot DWC3 core init */
	int ret = dwc3_uboot_init(&dwc3_dev);
	if (ret) {
		printf("dwc3_uboot_init failed: %d\n", ret);
		return NULL;
	}

	// GSNPSID (DWC3 revision: must be >= 0x5533220a for HS-capable DCFG.DEVSPD)
	printf("GSNPSID:    0x%08x\n", *(uint32_t *)(USB3DRD_BASE_ADDR + 0xc120));

	// GSBUSCFG0, GSBUSCFG1
	printf("0x4830C100: 0x%08x\n", *(uint32_t *)(0x4830C100));
	printf("0x4830C104: 0x%08x\n", *(uint32_t *)(0x4830C104));
	// U-boot: 0x4830c100:	0x00000009	0x00000300

	// GCTL
	printf("0x4830C110: 0x%08x\n", *(uint32_t *)(0x4830C110));
	// U-boot: 0x4830c110:	0x30c12004

	// GUSB2PHYCFG(0)
	printf("0x4830C200: 0x%08x\n", *(uint32_t *)(0x4830C200));
	// U-boot: 0x4830c200:	0x40002440

	// GUSB3PIPECTL(0)
	printf("0x4830C2C0: 0x%08x\n", *(uint32_t *)(0x4830C2C0));
	// U-boot: 0x4830c2c0:	0x000e0002

	// GTXFIFOSIZ 0-3
	printf("0x4830C300: 0x%08x\n", *(uint32_t *)(0x4830C300));
	printf("0x4830C304: 0x%08x\n", *(uint32_t *)(0x4830C304));
	printf("0x4830C308: 0x%08x\n", *(uint32_t *)(0x4830C308));
	printf("0x4830C30C: 0x%08x\n", *(uint32_t *)(0x4830C30C));
	// U-boot: 0x4830c300:	0x00000042	0x00420184	0x01c60184	0x034a0184

	// GRXFIFOSIZ 0-3
	printf("0x4830C380: 0x%08x\n", *(uint32_t *)(0x4830C380));
	printf("0x4830C384: 0x%08x\n", *(uint32_t *)(0x4830C384));
	printf("0x4830C388: 0x%08x\n", *(uint32_t *)(0x4830C388));
	printf("0x4830C38C: 0x%08x\n", *(uint32_t *)(0x4830C38C));
	// U-boot: 0x4830c380:	0x00000185	0x01850000	0x01850000	0x00000000

	// GEVNTADRLO/HI, GEVNTSIZ, GEVNTCOUNT
	printf("0x4830C400: 0x%08x\n", *(uint32_t *)(0x4830C400));
	printf("0x4830C404: 0x%08x\n", *(uint32_t *)(0x4830C404));
	printf("0x4830C408: 0x%08x\n", *(uint32_t *)(0x4830C408));
	printf("0x4830C40C: 0x%08x\n", *(uint32_t *)(0x4830C40C));
	// U-boot: 0x4830c400:	0xf80e1340	0x00000000	0x00000100	0x00000000

	// DCFG, DCTL, DEVTEN, DSTS, CMD, DALEPENA range
	printf("0x4830C700: 0x%08x\n", *(uint32_t *)(0x4830C700));
	printf("0x4830C704: 0x%08x\n", *(uint32_t *)(0x4830C704));
	printf("0x4830C708: 0x%08x\n", *(uint32_t *)(0x4830C708));
	printf("0x4830C70C: 0x%08x\n", *(uint32_t *)(0x4830C70C));
	printf("0x4830C710: 0x%08x\n", *(uint32_t *)(0x4830C710));
	printf("0x4830C714: 0x%08x\n", *(uint32_t *)(0x4830C714));
	printf("0x4830C718: 0x%08x\n", *(uint32_t *)(0x4830C718));
	printf("0x4830C71C: 0x%08x\n", *(uint32_t *)(0x4830C71C));
	// U-boot: 0x4830c700:	0x00480830	0x8c000a00	0x0000121f	0x00823a30
	// U-boot: 0x4830c710:	0x00000000	0x00000000	0x00000000	0x00000000
	// Bareml: 0x4830C700:  0x00080804  0x00000000  0x00000000  0x00d206fc

	/* Configure AHB burst mode: INCR4 only (matches stm32mp257f-ev1-revB DTS).
	 * Leaves undefined-length INCR disabled to avoid RISAF/AHB5 bus issues. */
	{
		volatile uint32_t *gsbuscfg0 = (volatile uint32_t *)(USB3DRD_BASE_ADDR + 0xc100);
		*gsbuscfg0 = (*gsbuscfg0 & ~0xffu) | (1u << 1); /* INCR4BRSTENA only */
	}

	/* Retrieve the dwc3 pointer */
	struct dwc3 *dwc = dwc3_uboot_get(dwc3_dev.index);
	if (!dwc) {
		printf("dwc3_uboot_get failed\n");
		return NULL;
	}

	/* GUCTL1 configuration — present in U-Boot's dwc3_init() (DM path)
	 * but missing from dwc3_uboot_init() (non-DM path).
	 * Rev 3.30b >= 2.90a, so set DEV_L1_EXIT_BY_HW. */
	if (dwc->revision >= DWC3_REVISION_250A) {
		u32 reg = dwc3_readl(dwc->regs, DWC3_GUCTL1);
		if (dwc->revision >= DWC3_REVISION_290A)
			reg |= DWC3_GUCTL1_DEV_L1_EXIT_BY_HW;
		if (dwc->dis_tx_ipgap_linecheck_quirk)
			reg |= DWC3_GUCTL1_TX_IPGAP_LINECHECK_DIS;
		dwc3_writel(dwc->regs, DWC3_GUCTL1, reg);
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
