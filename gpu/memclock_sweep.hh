#pragma once
#include "etna.hh"
#include <cstdint>

// Clock-source experiment. The GPU has two input clocks: ck_ker_gpu (PLL3, the
// nominal "800 MHz core clock") and ck_icn_m_gpu (flexgen59, the "bus/AXI clock",
// ~600 MHz). Changing PLL3 (800<->10 MHz) was observed to NOT change performance,
// yet FSCALE (which scales the GPU's internal clock) does. That means the real
// work clock isn't PLL3. This sweep retunes the BUS clock (flexgen59) across a
// range and times an RS fill + a shader op at each: if throughput tracks the bus
// clock, ck_icn_m_gpu is the actual GPU work clock and PLL3 is a phantom.
// Restores the bus clock to `restore_hz` when done.
void memclock_sweep(etna::Gpu &g, const etna::Bo &fb, uint32_t restore_hz = 600'000'000);

// Measure the GPU's ACTUAL core clock in MHz by timing a known number of FE
// WAIT cycles (cmd_wait stalls the front-end for N core-clock cycles). Works
// even though HI_TOTAL_CYCLES is blocked. Uses two batch sizes to cancel the
// fixed submit/IRQ overhead. Reflects the current FSCALE setting.
uint32_t measure_gpu_core_clock_mhz(etna::Gpu &g);

// Dump the actual RCC PLL3 / flexgen59 / GPUCFGR register state so we can see
// the GPU's real clock frequencies (vs. the computed values the setup prints).
void print_gpu_clock_regs();
