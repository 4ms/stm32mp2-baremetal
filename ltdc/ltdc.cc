#include "ltdc.hh"
#include "aarch64/system_reg.hh"  // read_cntpct/read_cntfreq (vblank timeout)
#include "drivers/hal_cnt.hh"	  // udelay
#include "interrupt/interrupt.hh" // InterruptManager (LTDC LINE IRQ)
#include "panel_etml0700z9.hh"
#include "stm32mp2xx.h"

// =============================================================================
//  ltdc.cc -- LTDC timings + one ARGB8888 layer (MP25, HW version 4.x)
// =============================================================================
// Ported from Linux drivers/gpu/drm/stm/ltdc.c (v6.6-stm32mp). Line references
// cite ltdc.c. MP25 specifics that differ from older STM32 LTDCs:
//   - layer registers latch ONLY via the per-layer reload register (L1RCR):
//     IMR (bit0) = immediate, VBR (bit1) = at next vblank. SRCR is not used?
//   - 64-bit AXI bus (affects the CFBLR line-length field).

namespace
{
using namespace Panel;

// GCR bits (ltdc.c:139-146)
constexpr uint32_t GCR_LTDCEN = 1u << 0;
constexpr uint32_t GCR_DEN = 1u << 16; // dither
constexpr uint32_t GCR_PCPOL = 1u << 28;
constexpr uint32_t GCR_DEPOL = 1u << 29;
constexpr uint32_t GCR_VSPOL = 1u << 30;
constexpr uint32_t GCR_HSPOL = 1u << 31;

// IER/ISR bits (ltdc.c:188-205)
constexpr uint32_t IT_LINE = 1u << 0; // LIE
constexpr uint32_t IT_FUW = 1u << 1;  // FIFO underrun warning
constexpr uint32_t IT_TERR = 1u << 2; // transfer error
constexpr uint32_t IT_RR = 1u << 3;	  // register reload
constexpr uint32_t IT_FUE = 1u << 6;  // FIFO underrun error

// Layer control / reload (ltdc.c:211, 253-255)
constexpr uint32_t LXCR_LEN = 1u << 0;
constexpr uint32_t LXRCR_IMR = 1u << 0;
constexpr uint32_t LXRCR_VBR = 1u << 1;

// Blend factors: BF1 = pixel-alpha x const-alpha, BF2 = 1 - that (ltdc.c:260-263)
constexpr uint32_t BF1_PAXCA = 0x600;
constexpr uint32_t BF2_1PAXCA = 0x007;

// vblank counter, bumped by the LTDC IRQ handler. Used
// for profiling/metrics.
volatile uint32_t g_vblank = 0;

Callback vblank_cb = [] {};

void ltdc_on_irq()
{
	uint32_t isr = LTDC->ISR;
	LTDC->ICR = isr; // clear everything we took (write-1-to-clear)
	if (isr & IT_LINE) {
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
		h = VActive; // 0 -> full screen (window == active area)

	// --- timings (formulas ltdc.c:1183-1209; values in panel_etml0700z9.hh) --
	LTDC->SSCR = (SyncW << 16) | SyncH;			// 0x00140014
	LTDC->BPCR = (AccumHbp << 16) | AccumVbp;	// 0x00AA002C
	LTDC->AWCR = (AccumActW << 16) | AccumActH; // 0x04AA0284
	LTDC->TWCR = (TotalW << 16) | TotalH;		// 0x0540029C
	LTDC->LIPCR = AccumActH + 1;				// line IRQ at start of vblank [1209]

	// --- polarities: EV1 panel-timing has no active-edge overrides, so all 0
	// (HS/VS active-low, DE active-high, pixel clock non-inverted; the syncs are
	// embedded in the LVDS stream anyway). No dithering. [ltdc.c:1211-1230]
	LTDC->GCR &= ~(GCR_HSPOL | GCR_VSPOL | GCR_DEPOL | GCR_PCPOL | GCR_DEN);

	LTDC->BCCR = bg_argb & 0xFFFFFF; // background color shown outside the layer window
	// EDCR (0x60): no YCbCr output conversion [1255]; FUT (0x90): underrun
	// warning threshold, driver default 128 [1274]. Raw offsets -- not in the
	// CMSIS struct on all header versions.
	*reinterpret_cast<volatile uint32_t *>(LTDC_BASE + 0x60) = 0;
	*reinterpret_cast<volatile uint32_t *>(LTDC_BASE + 0x90) = 0x80;

	// Only the LINE interrupt -- it fires once per frame at the start of vblank and
	// drives ltdc_wait_vblank(). (No FIFO/error/reload IRQs: reload never fires on
	// MP25 anyway, and error IRQs would just add noise.)
	LTDC->ICR = 0x3F;
	LTDC->IER = IT_LINE;
	InterruptManager::register_and_start_isr(LTDC_IRQn, 0, 0, [] { ltdc_on_irq(); });

	// --- layer 1: a w x h ARGB8888 window at (x, y) in the active area (defaults
	// to full-screen). ltdc_plane_update, ltdc.c:1786-2015. --
	LTDC_Layer1->WHPCR = ((AccumHbp + x + w) << 16) | (AccumHbp + x + 1); // [1801-1808]
	LTDC_Layer1->WVPCR = ((AccumVbp + y + h) << 16) | (AccumVbp + y + 1); // [1810-1816]
	LTDC_Layer1->PFCR = 0;												  // ARGB8888 [318-320]
	LTDC_Layer1->CACR = 0xFF;											  // constant alpha 1.0 [1818]
	LTDC_Layer1->DCCR = 0;												  // default color [1823]
	LTDC_Layer1->BFCR = BF1_PAXCA | BF2_1PAXCA;							  // 0x607 [1826-1838]
	LTDC_Layer1->BLCR = 0;												  // burst len: max [1963]
	LTDC_Layer1->CFBAR = fb_addr;										  // [1845]
	// CFBLR: pitch<<16 | (pitch + bus_width/8 - 1), 64-bit bus [1856-1866]
	LTDC_Layer1->CFBLR = ((w * 4) << 16) | (w * 4 + 8 - 1);
	LTDC_Layer1->CFBLNR = h;	  // [1868]
	LTDC_Layer1->CR = LXCR_LEN;	  // layer enable [2007]
	LTDC_Layer1->RCR = LXRCR_IMR; // latch NOW (MP25!) [2012]

	LTDC->GCR |= GCR_LTDCEN; // controller on -- last [1278]
}

void ltdc_set_framebuffer(uint32_t fb_addr)
{
	LTDC_Layer1->CFBAR = fb_addr;
	LTDC_Layer1->RCR = LXRCR_VBR; // latch at next vblank (tear-free)
}

void ltdc_set_callback(Callback &&cb)
{
	vblank_cb = std::move(cb);
}

bool ltdc_wait_vblank()
{
	// Block until the next vblank (the LTDC LINE interrupt bumps g_vblank at the
	// start of every frame). The VBR flip we armed latches at that same vblank, so
	// this paces exactly one rendered+displayed frame per refresh, tear-free -- and
	// the CPU sits in WFE meanwhile instead of polling scan-out registers. ~25 ms
	// deadline in case the IRQ never arrives.
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
	return LTDC->CPSR & 0xFFFF; // CYPOS [ltdc.c:197]
}

uint32_t ltdc_isr()
{
	return LTDC->ISR;
}

uint32_t ltdc_hw_version()
{
	return LTDC->IDR;
}
