#include "ltdc.hh"
#include "aarch64/system_reg.hh"  // read_cntpct/read_cntfreq (vblank timeout)
#include "interrupt/interrupt.hh" // InterruptManager (LTDC LINE IRQ)
#include "panel_nv3052c.hh"
#include "stm32mp2xx_hal.h"
#include <atomic>

namespace
{
using namespace Panel;

LTDC_HandleTypeDef g_ltdc;

std::atomic<uint32_t> g_vblank = 0; // bumped by the LINE IRQ, drives ltdc_wait_vblank()
Callback vblank_cb = [] {};

void ltdc_on_irq()
{
	uint32_t isr = LTDC->ISR;
	LTDC->ICR = isr; // write-1-to-clear everything we took
	if (isr & LTDC_ISR_LIF) {
		g_vblank = g_vblank + 1;
		vblank_cb();
	}
}
} // namespace

void ltdc_init(uint32_t fb_addr, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg_argb)
{
	if (w == 0)
		w = HActive;
	if (h == 0)
		h = VActive; // 0 -> full screen

	// --- controller: timings + polarities (values from panel_nv3052c.hh) ------
	g_ltdc.Instance = LTDC;
	g_ltdc.Init.HSPolarity = HSyncActiveHigh ? LTDC_HSPOLARITY_AH : LTDC_HSPOLARITY_AL;
	g_ltdc.Init.VSPolarity = VSyncActiveHigh ? LTDC_VSPOLARITY_AH : LTDC_VSPOLARITY_AL;
	g_ltdc.Init.DEPolarity = DEActiveHigh ? LTDC_DEPOLARITY_AL : LTDC_DEPOLARITY_AH; // see polarity note
	g_ltdc.Init.PCPolarity = PixDataDriveNegEdge ? LTDC_PCPOLARITY_IIPC : LTDC_PCPOLARITY_IPC;
	g_ltdc.Init.HorizontalSync = SyncW;
	g_ltdc.Init.VerticalSync = SyncH;
	g_ltdc.Init.AccumulatedHBP = AccumHbp;
	g_ltdc.Init.AccumulatedVBP = AccumVbp;
	g_ltdc.Init.AccumulatedActiveW = AccumActW;
	g_ltdc.Init.AccumulatedActiveH = AccumActH;
	g_ltdc.Init.TotalWidth = TotalW;
	g_ltdc.Init.TotalHeigh = TotalH;
	g_ltdc.Init.Backcolor.Red = (bg_argb >> 16) & 0xFF;
	g_ltdc.Init.Backcolor.Green = (bg_argb >> 8) & 0xFF;
	g_ltdc.Init.Backcolor.Blue = bg_argb & 0xFF;
	g_ltdc.Init.FifoUnderThresh = 0x80; // driver-default underrun warning threshold
	HAL_LTDC_Init(&g_ltdc);				// programs GCR/timings + enables LTDCEN

	// HAL_LTDC_Init turns on TE/FU (and the secure copies) but we
	// only want the LINE interrupt configured to fire once per frame at start of vblank
	LTDC->IER = LTDC_IER_LIE;
	LTDC->IER2 = 0; // secure IER off
	LTDC->ICR = 0x3F;
	LTDC->LIPCR = AccumActH + 1; // line IRQ at start of vblank

	// --- layer 1: a w x h RGB565/ARGB8888 window at (x, y); HAL adds the accumulated
	// back-porch offsets internally, so pass raw positions. --
	LTDC_LayerCfgTypeDef lc{};
	lc.WindowX0 = x;
	lc.WindowX1 = x + w;
	lc.WindowY0 = y;
	lc.WindowY1 = y + h;
	lc.PixelFormat = BytesPerPixel == 4 ? LTDC_PIXEL_FORMAT_ARGB8888 : LTDC_PIXEL_FORMAT_RGB565;
	lc.Alpha = 0xFF; // constant alpha 1.0
	lc.Alpha0 = 0;
	lc.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
	lc.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
	lc.FBStartAdress = fb_addr;
	lc.ImageWidth = w;
	lc.ImageHeight = h;
	lc.BurstLength = 0; // max burst
	HAL_LTDC_ConfigLayer(&g_ltdc, &lc, LTDC_LAYER_1);

	// ST Hal writes a negative CFBLR pitch (probably copied from the rotation-enabled code.
	// Write the correct value here:
	// CFBLR = pitch<<16 | (line_bytes + bus_width_bytes - 1); 64-bit bus -> +7.
	LTDC_Layer1->CFBLR = ((w * BytesPerPixel) << 16) | (w * BytesPerPixel + 8 - 1);
	LTDC_Layer1->RCR = LTDC_LxRCR_IMR; // per-layer immediate reload

	InterruptManager::register_and_start_isr(LTDC_IRQn, 0, 0, [] { ltdc_on_irq(); });
}

void ltdc_set_framebuffer(uint32_t fb_addr)
{
	// Tear-free flip: new address + per-layer reload at the next vblank. Done by
	// hand (not HAL_LTDC_SetAddress) to avoid re-running the HAL's buggy pitch.
	LTDC_Layer1->CFBAR = fb_addr;
	LTDC_Layer1->RCR = LTDC_LxRCR_VBR;
}

void ltdc_set_callback(Callback &&cb)
{
	vblank_cb = std::move(cb);
}

bool ltdc_wait_vblank()
{
	// Block until the next vblank (the LINE IRQ bumps g_vblank at the start of
	// every frame). ~25 ms deadline in case the IRQ never arrives.
	uint32_t start = g_vblank;
	uint64_t deadline = read_cntpct() + read_cntfreq() / 40; // ~25 ms
	while (g_vblank == start) {
		asm volatile("wfe");
		if (read_cntpct() > deadline)
			return false;
	}
	return true;
}

uint32_t ltdc_current_line()
{
	return LTDC->CPSR & 0xFFFF; // CYPOS
}

uint32_t ltdc_isr()
{
	return LTDC->ISR;
}

uint32_t ltdc_hw_version()
{
	return LTDC->IDR;
}
