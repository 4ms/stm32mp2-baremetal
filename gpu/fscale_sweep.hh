#pragma once
#include "etna.hh"
#include <cstdint>

// Characterize HI_CLOCK_CONTROL.FSCALE_VAL: for each value, measure the actual
// GPU core clock (HI_TOTAL_CYCLES), core busy fraction (HI_TOTAL_IDLE_CYCLES),
// and the resulting RS-fill throughput + DDR stats. Resolves what FSCALE really
// does on this ST core and whether the core clock (vs the write-issue path) is
// what caps the fill. Restores FSCALE to `restore` when done.
void fscale_sweep(etna::Gpu &g, const etna::Bo &fb, uint32_t restore = 0x00);
