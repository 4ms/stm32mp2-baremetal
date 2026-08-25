#pragma once
#include "interrupt/callable.hh"
#include <cstdint>

// The layer is a w x h window, with w or h = 0 meaning full-screen
// `bg_argb` (RGB) fills the area outside the window. Layer pixel format is
// RGB565 or ARGB8888 per Panel::BytesPerPixel (panel_nv3052c.hh).
void ltdc_init(uint32_t fb_addr, uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0, uint32_t bg_argb = 0);

// Point the layer at a new framebuffer and latch on the next vblank (tear-free).
void ltdc_set_framebuffer(uint32_t fb_addr);

uint32_t ltdc_current_line();
uint32_t ltdc_isr();
uint32_t ltdc_hw_version();

bool ltdc_wait_vblank();
void ltdc_set_callback(Callback &&cb);
