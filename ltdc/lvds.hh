#pragma once
#include <cstdint>

// LVDS host + PHY (0x48060000) bring-up, ported from Linux drivers/gpu/drm/stm/
// lvds.c for the MP25. Single-link, primary PHY only, vesa-24 (VESA RGB888).

// Program + lock the PHY PLL (378 MHz bit clock from the 40 MHz HSE ref).
// Call after the LVDS bus clock is on, reset released, and the ref mux set to
// HSE. Returns false if the PLL never locks.
bool lvds_pll_init();

// Configure the host (single link, channel distribution, vesa-24 data mapping)
// and turn the LVDS output on. Call after the LTDC is running.
void lvds_host_enable();

// The IP version register (expect a 0x1x value worth printing at bring-up).
uint32_t lvds_version();
