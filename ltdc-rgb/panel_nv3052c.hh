#pragma once
#include <cstdint>

// =============================================================================
//  panel_nv3052c.hh -- ER-TFT3.95-1 (NV3052C) 720x720 parallel-RGB panel
// =============================================================================
//
// Bus width: the devboard has LTDC R/G/B[7:2] connected to the panel
//
// Timings: the NV3052C datasheet has no 720x720 table (only 720x1280), so the
// porches are the ones Adafruit ships for their NV3052C 720x720 panels
// (HSW 2 / HBP 44 / HFP 46, VSW 2 / VBP 18 / VFP 16), which are also close to
// the datasheet's 720-wide row (hpw 2 / hbp 42 / hfp 44). The controller is
// run in DE mode (reg 23h = 0x20 in the vendor init), so the porches only need
// to be close, not exact.
//
// Pixel clock: datasheet min PCLK cycle is 28 ns (35.7 MHz max); 812x756 at
// 60 Hz would need 36.8 MHz, so target ~34 MHz -> ~55 Hz. display.cc picks the
// nearest PLL4 divider at runtime.

namespace Panel
{

// ---- display mode ----------------------------------------------------------
constexpr uint32_t HActive = 720;
constexpr uint32_t HFront = 46; // HFP
constexpr uint32_t HSync = 2;	// HSW (HPW)
constexpr uint32_t HBack = 44;	// HBP

constexpr uint32_t VActive = 720;
constexpr uint32_t VFront = 16; // VFP
constexpr uint32_t VSync = 2;	// VSW
constexpr uint32_t VBack = 18;	// VBP

constexpr uint32_t HTotal = HActive + HFront + HSync + HBack; // 812
constexpr uint32_t VTotal = VActive + VFront + VSync + VBack; // 756

constexpr uint32_t MaxPixelClockHz = 34'000'000; // panel max is 35.7 MHz (28 ns)

// ---- signal polarities (NV3052C reg 23h defaults: vspl=hspl=dpl=epl=0) ------
// VS/HS: low-active. DE: high-active. Data: fetched by the panel on the RISING
// PCLK edge, so the LTDC must drive it on the falling edge.
constexpr bool HSyncActiveHigh = false;
constexpr bool VSyncActiveHigh = false;
constexpr bool DEActiveHigh = true;
constexpr bool PixDataDriveNegEdge = true; // LTDC GCR.PCPOL

// ---- framebuffer -----------------------------------------------------------
// 2 = RGB565 (this example), 4 = ARGB8888. Projects that reuse this display
// path with a 32-bit renderer (gpu-ltdc-demo) override it from their Makefile
// with -DLTDC_RGB_BYTES_PER_PIXEL=4; ltdc.cc picks the layer format from it.
#ifndef LTDC_RGB_BYTES_PER_PIXEL
#define LTDC_RGB_BYTES_PER_PIXEL 2
#endif
constexpr uint32_t BytesPerPixel = LTDC_RGB_BYTES_PER_PIXEL;
static_assert(BytesPerPixel == 2 || BytesPerPixel == 4, "RGB565 or ARGB8888 only");
constexpr uint32_t Pitch = HActive * BytesPerPixel;
constexpr uint32_t FbBytes = Pitch * VActive;

// ---- LTDC accumulated-timing register fields (same formulas as ltdc.c) ------
constexpr uint32_t SyncW = HSync - 1;			   // 1
constexpr uint32_t SyncH = VSync - 1;			   // 1
constexpr uint32_t AccumHbp = HSync + HBack - 1;   // 45
constexpr uint32_t AccumVbp = VSync + VBack - 1;   // 19
constexpr uint32_t AccumActW = AccumHbp + HActive; // 765
constexpr uint32_t AccumActH = AccumVbp + VActive; // 739
constexpr uint32_t TotalW = HTotal - 1;			   // 811
constexpr uint32_t TotalH = VTotal - 1;			   // 755

} // namespace Panel
