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

// Block until the last ltdc_set_framebuffer() flip actually latched (next
// vblank). Call before rendering into the just-retired front buffer --
// this is what makes double-buffering tear-free. False on timeout (~25 ms).
bool ltdc_wait_reload();

// Layer 2: a w x h ARGB8888 window at (x, y) in the active area, composited
// over layer 1 during scanout. For putting a small per-frame surface (e.g. a
// spinning cube) on top of a static full-screen background layer 1.
void ltdc_layer2_init(uint32_t fb_addr, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void ltdc_layer2_set_framebuffer(uint32_t fb_addr); // vblank-latched flip

uint32_t ltdc_current_line(); // CPSR CYPOS -- advancing == pixel clock alive
uint32_t ltdc_isr();		  // ISR flags (bit1 FIFO warn, 2 transfer err, 6 FIFO err)
uint32_t ltdc_hw_version();	  // IDR, expect 0x0401xx on MP25
