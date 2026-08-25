#pragma once
#include <cstdint>

void display_pins_setup();
void display_rif_setup();
uint32_t display_clocks_setup();

bool display_init(
	uint32_t first_fb, uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0, uint32_t bg_argb = 0);
