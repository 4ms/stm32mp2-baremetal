#pragma once
#include <cstdint>

// Board-level display bring-up for the EV1 + B-LVDS7-WSVGA panel, shared by
// the ltdc/ example and gpu-ltdc-demo/. The pieces are individually callable
// (so an example can print between stages)

void display_rif_setup();	 // RISUP 80/84/119 + RIMU 11 (LTDC DMA master) secure
void display_clocks_setup(); // gates + reset pulses + PHY ref = HSE 40 MHz
void display_panel_on();	 // PG15 panel enable, then PI5 backlight

bool display_init(
	uint32_t first_fb, uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0, uint32_t bg_argb = 0);
