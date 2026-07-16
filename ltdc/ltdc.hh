#pragma once
#include <cstdint>

// LTDC (0x48010000) config for the MP25 (HW version 4.x: per-layer shadow
// reload, 64-bit bus). Ported from Linux drivers/gpu/drm/stm/ltdc.c.

// Program timings + one full-screen ARGB8888 layer scanning out `fb_addr`,
// latch the layer registers, and enable the controller. The pixel clock (from
// the LVDS PHY via the SYSCFG mux) must be ticking first.
void ltdc_init(uint32_t fb_addr);

// Point layer 1 at a new framebuffer and latch on the next vblank (for
// animation later; ltdc_init uses an immediate latch).
void ltdc_set_framebuffer(uint32_t fb_addr);

uint32_t ltdc_current_line(); // CPSR CYPOS -- advancing == pixel clock alive
uint32_t ltdc_isr();		  // ISR flags (bit1 FIFO warn, 2 transfer err, 6 FIFO err)
uint32_t ltdc_hw_version();	  // IDR, expect 0x0401xx on MP25
