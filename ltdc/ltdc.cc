#include "ltdc.hh"
#include "drivers/hal_cnt.hh" // udelay
#include "panel_etml0700z9.hh"
#include "stm32mp2xx.h"

// =============================================================================
//  ltdc.cc -- LTDC timings + one ARGB8888 layer (MP25, HW version 4.x)
// =============================================================================
// Ported from Linux drivers/gpu/drm/stm/ltdc.c (v6.6-stm32mp). Line references
// cite ltdc.c. MP25 specifics that differ from older STM32 LTDCs:
//   - layer registers latch ONLY via the per-layer reload register (L1RCR):
//     IMR (bit0) = immediate, VBR (bit1) = at next vblank. The global SRCR
//     path is not used. Forgetting this = perfect timings, black screen.
//   - 64-bit AXI bus (affects the CFBLR line-length field).

namespace
{
using namespace Panel;

// GCR bits (ltdc.c:139-146)
constexpr uint32_t GCR_LTDCEN = 1u << 0;
constexpr uint32_t GCR_DEN = 1u << 16;	  // dither
constexpr uint32_t GCR_PCPOL = 1u << 28;
constexpr uint32_t GCR_DEPOL = 1u << 29;
constexpr uint32_t GCR_VSPOL = 1u << 30;
constexpr uint32_t GCR_HSPOL = 1u << 31;

// IER/ISR bits (ltdc.c:188-205)
constexpr uint32_t IT_LINE = 1u << 0;  // LIE
constexpr uint32_t IT_FUW = 1u << 1;   // FIFO underrun warning
constexpr uint32_t IT_TERR = 1u << 2;  // transfer error
constexpr uint32_t IT_RR = 1u << 3;	   // register reload
constexpr uint32_t IT_FUE = 1u << 6;   // FIFO underrun error

// Layer control / reload (ltdc.c:211, 253-255)
constexpr uint32_t LXCR_LEN = 1u << 0;
constexpr uint32_t LXRCR_IMR = 1u << 0;
constexpr uint32_t LXRCR_VBR = 1u << 1;

// Blend factors: BF1 = pixel-alpha x const-alpha, BF2 = 1 - that (ltdc.c:260-263)
constexpr uint32_t BF1_PAXCA = 0x600;
constexpr uint32_t BF2_1PAXCA = 0x007;
} // namespace

void ltdc_init(uint32_t fb_addr)
{
	// --- timings (formulas ltdc.c:1183-1209; values in panel_etml0700z9.hh) --
	LTDC->SSCR = (SyncW << 16) | SyncH;			 // 0x00140014
	LTDC->BPCR = (AccumHbp << 16) | AccumVbp;	 // 0x00AA002C
	LTDC->AWCR = (AccumActW << 16) | AccumActH;	 // 0x04AA0284
	LTDC->TWCR = (TotalW << 16) | TotalH;		 // 0x0540029C
	LTDC->LIPCR = AccumActH + 1;				 // line IRQ at start of vblank [1209]

	// --- polarities: EV1 panel-timing has no active-edge overrides, so all 0
	// (HS/VS active-low, DE active-high, pixel clock non-inverted; the syncs are
	// embedded in the LVDS stream anyway). No dithering. [ltdc.c:1211-1230]
	LTDC->GCR &= ~(GCR_HSPOL | GCR_VSPOL | GCR_DEPOL | GCR_PCPOL | GCR_DEN);

	LTDC->BCCR = 0x00000000; // background black [1262]
	// EDCR (0x60): no YCbCr output conversion [1255]; FUT (0x90): underrun
	// warning threshold, driver default 128 [1274]. Raw offsets -- not in the
	// CMSIS struct on all header versions.
	*reinterpret_cast<volatile uint32_t *>(LTDC_BASE + 0x60) = 0;
	*reinterpret_cast<volatile uint32_t *>(LTDC_BASE + 0x90) = 0x80;

	LTDC->IER = IT_FUW | IT_TERR | IT_FUE | IT_LINE | IT_RR; // [1266] + line/reload

	// --- layer 1: full-screen ARGB8888 (ltdc_plane_update, ltdc.c:1786-2015) --
	LTDC_Layer1->WHPCR = (AccumActW << 16) | (AccumHbp + 1); // x window [1801-1808]
	LTDC_Layer1->WVPCR = (AccumActH << 16) | (AccumVbp + 1); // y window [1810-1816]
	LTDC_Layer1->PFCR = 0;									 // ARGB8888 [318-320]
	LTDC_Layer1->CACR = 0xFF;								 // constant alpha 1.0 [1818]
	LTDC_Layer1->DCCR = 0;									 // default color [1823]
	LTDC_Layer1->BFCR = BF1_PAXCA | BF2_1PAXCA;				 // 0x607 [1826-1838]
	LTDC_Layer1->BLCR = 0;									 // burst len: max [1963]
	LTDC_Layer1->CFBAR = fb_addr;							 // [1845]
	// CFBLR: pitch<<16 | (pitch + bus_width/8 - 1), 64-bit bus [1856-1866]
	LTDC_Layer1->CFBLR = ((HActive * 4) << 16) | (HActive * 4 + 8 - 1); // 0x10001007
	LTDC_Layer1->CFBLNR = VActive;							 // [1868]
	LTDC_Layer1->CR = LXCR_LEN;								 // layer enable [2007]
	LTDC_Layer1->RCR = LXRCR_IMR;							 // latch NOW (MP25!) [2012]

	LTDC->GCR |= GCR_LTDCEN; // controller on -- last [1278]
}

void ltdc_set_framebuffer(uint32_t fb_addr)
{
	LTDC->ICR = IT_RR; // clear the reload flag so ltdc_wait_reload() sees THIS flip
	LTDC_Layer1->CFBAR = fb_addr;
	LTDC_Layer1->RCR = LXRCR_VBR; // latch at next vblank (tear-free)
}

// Set up layer 2 as a w x h ARGB8888 window whose top-left sits at (x, y) in
// the active area, scanning out `fb_addr`. The LTDC composites it over layer 1
// during scanout -- so a small per-frame layer (the cube) rides on top of a
// static full-screen layer 1 (the background), and only the small window's
// bytes are produced each frame. Latches immediately.
void ltdc_layer2_init(uint32_t fb_addr, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	// Window position: same accumulated-porch offset as layer 1, plus (x, y).
	// First/last column & row are inclusive, 1-based within the active area.
	LTDC_Layer2->WHPCR = ((AccumHbp + x + w) << 16) | (AccumHbp + x + 1);
	LTDC_Layer2->WVPCR = ((AccumVbp + y + h) << 16) | (AccumVbp + y + 1);
	LTDC_Layer2->PFCR = 0;	 // ARGB8888
	LTDC_Layer2->CACR = 0xFF; // constant alpha 1.0
	LTDC_Layer2->DCCR = 0;
	LTDC_Layer2->BFCR = BF1_PAXCA | BF2_1PAXCA; // pixel-alpha blend (cube is opaque)
	LTDC_Layer2->BLCR = 0;
	LTDC_Layer2->CFBAR = fb_addr;
	LTDC_Layer2->CFBLR = ((w * 4) << 16) | (w * 4 + 8 - 1); // pitch = w*4, 64-bit bus
	LTDC_Layer2->CFBLNR = h;
	LTDC_Layer2->CR = LXCR_LEN;
	LTDC_Layer2->RCR = LXRCR_IMR; // latch now
}

void ltdc_layer2_set_framebuffer(uint32_t fb_addr)
{
	LTDC->ICR = IT_RR;
	LTDC_Layer2->CFBAR = fb_addr;
	LTDC_Layer2->RCR = LXRCR_VBR; // latch at next vblank
}

bool ltdc_wait_reload()
{
	// Block until the next active->vblank edge of the scan-out (CYPOS crossing into
	// the front porch at line AccumActH). The layer VBR flip we armed latches at
	// that vblank, so this paces exactly one rendered+displayed frame per refresh
	// -> tear-free. Detecting the EDGE (two phases) is race-free: unlike a "clear
	// the flag then poll it" scheme (IT_RR never fires on MP25; IT_LINE could read
	// already-set right after the clear and return early -> a dropped frame), there
	// is no flag to catch stale, so no early return and no phase drift.
	// Phase 1: make sure we start OUTSIDE vblank (in active/back-porch region).
	for (int i = 0; i < 25000 && (LTDC->CPSR & 0xFFFF) >= AccumActH; i++)
		udelay(1);
	// Phase 2: wait until the scan reaches the start of vblank.
	for (int i = 0; i < 25000; i++) {
		if ((LTDC->CPSR & 0xFFFF) >= AccumActH)
			return true;
		udelay(1);
	}
	return false;
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
