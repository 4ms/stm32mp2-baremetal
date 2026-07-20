#include "lvds.hh"
#include "drivers/hal_cnt.hh" // udelay
#include "panel_etml0700z9.hh"
#include "stm32mp2xx.h"

// =============================================================================
//  lvds.cc -- MP25 LVDS host + master-PHY bring-up (single link, vesa-24)
// =============================================================================
// 1:1 port of the enable path in Linux drivers/gpu/drm/stm/lvds.c (v6.6-stm32mp)
// onto the CMSIS LVDS_TypeDef register names. Line references cite lvds.c.
// Single-link primary ("PM*" master PHY at +0x1000); the slave PHY stays off.

namespace
{
// PMGCR bits (lvds.c PHY_GCR_*)
constexpr uint32_t GCR_DIV_RSTN = 1 << 25;
constexpr uint32_t GCR_RSTZ = 1 << 24;
constexpr uint32_t GCR_DP_CLK_OUT = 1 << 8;
constexpr uint32_t GCR_LS_CLK_OUT = 1 << 4;
constexpr uint32_t GCR_BIT_CLK_OUT = 1 << 0;

// PMPLLCR1 bits
constexpr uint32_t PLLCR1_PLL_EN = 1 << 0;
constexpr uint32_t PLLCR1_EN_TWG = 1 << 1;
constexpr uint32_t PLLCR1_EN_SD = 1 << 2;
constexpr uint32_t PLLCR1_DIV_EN = 1 << 8;

// Host CR bits
constexpr uint32_t CR_LVDSEN = 1 << 0;
} // namespace

bool lvds_pll_init()
{
	// --- Phase A: PHY reset release + PLL/analog config (lvds.c:450-551) -----
	LVDS->PMGCR |= GCR_DIV_RSTN | GCR_RSTZ;					   // release PHY digital reset [585]
	LVDS->PMPLLCR2 = Panel::PllNdiv << 16;					   // NDIV [510]
	LVDS->PMPLLCR2 |= Panel::PllBdiv;						   // BDIV [511]
	LVDS->PMPLLSDCR1 = Panel::PllMdiv;						   // MDIV [512]
	LVDS->PMPLLTESTCR = Panel::PllTdiv << 16;				   // TDIV [515]
	LVDS->PMPLLCR1 &= ~(PLLCR1_EN_TWG | PLLCR1_EN_SD);		   // integer mode [538]
	LVDS->PMDCR |= 1 << 12;									   // POWER_OK [541]
	LVDS->PMCMCR1 |= 0x10101010;							   // current-mode drivers DL0..3 [542]
	LVDS->PMCMCR2 |= 0x00000010;							   // current-mode driver DL4 [543]
	LVDS->PMPLLCPCR |= 1;									   // charge pump [546]
	LVDS->PMBCR3 |= 0x00011111;								   // voltage-mode drivers, 5 lanes [547]
	LVDS->PMBCR1 |= 0x00011111;								   // bias, 5 lanes [548]
	LVDS->PMCFGCR |= 0x0000001F;							   // digital datalanes [550]

	// --- Phase B: PLL enable + lock (lvds.c:351-391) --------------------------
	LVDS->PMMPLCR = (0x200 - 0x160) << 16;					   // monitor-PLL unmask window [361]
	LVDS->PMBCR2 = 0x10000000;								   // BIAS_EN [364]
	LVDS->PMGCR |= GCR_DP_CLK_OUT | GCR_LS_CLK_OUT | GCR_BIT_CLK_OUT; // [367-368]
	LVDS->PMPLLTESTCR |= 1 << 8;							   // dividers powered [371]
	LVDS->PMPLLCR1 |= PLLCR1_DIV_EN;						   // [372]
	LVDS->PMSCR |= 1 << 16;									   // TX_EN serial mode [375]
	LVDS->PMPLLCR1 |= PLLCR1_PLL_EN;						   // [378]

	// poll lock: 1 ms steps, 200 ms timeout (lvds.c:379-381)
	bool locked = false;
	for (int i = 0; i < 200; i++) {
		if (LVDS->PMPLLSR & 1) {
			locked = true;
			break;
		}
		udelay(1000);
	}
	if (!locked)
		return false;

	LVDS->WCLKCR = 1; // written unconditionally by the driver, even single-link [385-386]

	// lvds.c:388 writes PLLTESTCR bit0 but forgets the PHY base offset, so it
	// lands at host+0xE8. It ships that way everywhere and panels work.
	// Replicate the stray write AND set the intended bit, defensively.
	*reinterpret_cast<volatile uint32_t *>(LVDS_BASE + 0xE8) |= 1;
	LVDS->PMPLLTESTCR |= 1;

	return true;
}

void lvds_host_enable()
{
	// --- Phase C: host config + output enable (lvds.c:984-1036) --------------
	LVDS->CR = 0;			  // single link, no sync-polarity flips [840-875]
	LVDS->CDL1CR = 0x00004321; // link-1 lane distribution: ch 1..4 + clock [848]
	LVDS->CDL2CR = 0;		  // no secondary link [877]

	// vesa-24 (VESA RGB888) serializer bit mapping (lvds.c:340-346, 792-834)
	LVDS->DMLCR0 = 0x000FFFDE; // clock lane pattern 1100011
	LVDS->DMMCR0 = 0x00007BDF;
	LVDS->DMLCR1 = 0x000190A8; // R0-R5, G0
	LVDS->DMMCR1 = 0x00000022;
	LVDS->DMLCR2 = 0x00063611; // G1-G5, B0, B1
	LVDS->DMMCR2 = 0x0000254B;
	LVDS->DMLCR3 = 0x000AE33A; // B2-B5, HS, VS, DE
	LVDS->DMMCR3 = 0x00004A74;
	LVDS->DMLCR4 = 0x0007DAFB; // R6, R7, G6, G7, B6, B7, CE
	LVDS->DMMCR4 = 0x000018EE;

	LVDS->CR |= CR_LVDSEN; // output on [1026]
}

uint32_t lvds_version()
{
	return LVDS->VERR;
}
