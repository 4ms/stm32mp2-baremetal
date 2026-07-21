#pragma once
#include "interrupt/callable.hh"
#include <cstdint>

// LTDC (0x48010000) config for the MP25 (HW version 4.x: per-layer shadow
// reload, 64-bit bus). Ported from Linux drivers/gpu/drm/stm/ltdc.c.

// Program timings + one ARGB8888 layer scanning out `fb_addr`, latch it, enable
// the controller, and hook up the vblank (LINE) interrupt. The layer is a `w` x
// `h` window at (x, y) in the active area; w/h = 0 mean full-screen. `bg_argb`
// (RGB) fills the area outside the window. The pixel clock (LVDS PHY via the
// SYSCFG mux) must be ticking first.
void ltdc_init(uint32_t fb_addr, uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0, uint32_t bg_argb = 0);

// Point the layer at a new framebuffer and latch on the next vblank (tear-free).
void ltdc_set_framebuffer(uint32_t fb_addr);

uint32_t ltdc_current_line(); // CPSR CYPOS -- advancing == pixel clock alive
uint32_t ltdc_isr();		  // ISR flags (bit1 FIFO warn, 2 transfer err, 6 FIFO err)
uint32_t ltdc_hw_version();	  // IDR, expect 0x0401xx on MP25

bool ltdc_wait_vblank();
void ltdc_set_callback(Callback &&cb);
