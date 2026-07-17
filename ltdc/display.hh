#pragma once
#include <cstdint>

// Board-level display bring-up for the EV1 + B-LVDS7-WSVGA panel, shared by
// the ltdc/ example and gpu-ltdc-demo/. The pieces are individually callable
// (so an example can print between stages); display_init() runs the whole
// ladder: RIF -> clocks/resets -> LVDS PLL -> pixel-clock mux -> LTDC ->
// LVDS host -> panel + backlight GPIOs.

void display_rif_setup();	// RISUP 80/84/119 + RIMU 11 (LTDC DMA master) secure
void display_clocks_setup(); // gates + reset pulses + PHY ref = HSE 40 MHz
void display_panel_on();	 // PG15 panel enable, then PI5 backlight

// Full ladder, scanning out `first_fb` (linear ARGB8888, 1024x600).
// Returns false if the LVDS PLL fails to lock.
bool display_init(uint32_t first_fb);
