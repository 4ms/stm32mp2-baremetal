#pragma once
#include <cstdint>

// DDRPERFM probe measures DDR-controller-side traffic so we can tell whether
// the GPU RS-fill is bandwidth-bound (DDR saturated) or latency/serialization-
// bound (DDR mostly idle while the fill crawls). DDRPERFM counts all masters,
// in this example the A35 is parked on WFE during submit_and_wait, so
// the traffic captured around a fill is effectively the GPU's.
namespace perfmon
{
struct DdrSample {
	uint32_t writes; // EVCNT0 = PERF_OP_IS_WR (one BL8 write command)
	uint32_t reads;	 // EVCNT1 = PERF_OP_IS_RD (one BL8 read command)
	uint32_t acts;	 // EVCNT2 = PERF_OP_IS_ACT (row activate)
	uint32_t pres;	 // EVCNT3 = PERF_OP_IS_PRE (precharge)
	uint32_t tcnt;	 // free-running time counter (ticks once per BL8 burst)
	uint32_t status; // STATUS: bits [7:0] = per-counter overflow
};

// Program events + enable counters, leave stopped. Call once after DDR is up.
void ddr_init();

// Clear and start counting.
void ddr_start();

// Stop and snapshot the counters.
DdrSample ddr_stop();

// Pretty-print one sample with derived busy% / MB/s and a bound-vs-not verdict.
// expected_bytes: the known transfer size for the bracketed op (0 = unknown).
void ddr_report(const char *label, const DdrSample &s, uint64_t expected_bytes);
} // namespace perfmon
