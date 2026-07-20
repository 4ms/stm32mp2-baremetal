#pragma once
#include <cstdint>

// =============================================================================
//  panel_etml0700z9.hh -- EDT ETML0700Z9NDHA (B-LVDS7-WSVGA) timings
// =============================================================================
// From the stm32mp257f-ev1.dts panel-timing node: 54 MHz pixel clock,
// 1024x600@60, vesa-24 LVDS mapping. Panel enable = PG15, backlight = PI5
// (both active-high plain GPIOs). LTDC register values are derived here with
// the same formulas ltdc.c uses (drm mode -> accumulated-porch registers), so
// the arithmetic is visible instead of baked into magic constants.

namespace Panel
{

constexpr uint32_t PixelClockHz = 54'000'000;

constexpr uint32_t HActive = 1024;
constexpr uint32_t HFront = 150;
constexpr uint32_t HSync = 21;
constexpr uint32_t HBack = 150;

constexpr uint32_t VActive = 600;
constexpr uint32_t VFront = 24;
constexpr uint32_t VSync = 21;
constexpr uint32_t VBack = 24;

// DRM-style mode points
constexpr uint32_t HSyncStart = HActive + HFront;	// 1174
constexpr uint32_t HSyncEnd = HSyncStart + HSync;	// 1195
constexpr uint32_t HTotal = HSyncEnd + HBack;		// 1345
constexpr uint32_t VSyncStart = VActive + VFront;	// 624
constexpr uint32_t VSyncEnd = VSyncStart + VSync;	// 645
constexpr uint32_t VTotal = VSyncEnd + VBack;		// 669
// 54e6 / (1345*669) = 60.0 Hz

// LTDC accumulated-timing register fields (formulas from ltdc.c:1183-1209)
constexpr uint32_t SyncW = HSyncEnd - HSyncStart - 1;	// 20
constexpr uint32_t SyncH = VSyncEnd - VSyncStart - 1;	// 20
constexpr uint32_t AccumHbp = HTotal - HSyncStart - 1;	// 170
constexpr uint32_t AccumVbp = VTotal - VSyncStart - 1;	// 44
constexpr uint32_t AccumActW = AccumHbp + HActive;		// 1194
constexpr uint32_t AccumActH = AccumVbp + VActive;		// 644
constexpr uint32_t TotalW = HTotal - 1;					// 1344
constexpr uint32_t TotalH = VTotal - 1;					// 668

// LVDS PHY PLL solution for 54 MHz pixel from the 40 MHz HSE reference
// (search loop in lvds.c:412-448): bit clock = 7 * pixel = 378 MHz exactly.
//   Fbit = Fref * MDIV / (NDIV * BDIV) = 40e6 * 189 / (4*5) = 378 MHz
constexpr uint32_t PllNdiv = 4;
constexpr uint32_t PllBdiv = 5;
constexpr uint32_t PllMdiv = 189;
constexpr uint32_t PllTdiv = 70; // hardcoded by the driver (lvds.c:514-515)

} // namespace Panel
