#pragma once
#include <cstdint>

// DDRPERFM probe -- measures DDR-controller-side traffic so we can tell whether
// the GPU RS-fill is bandwidth-bound (DDR saturated) or latency/serialization-
// bound (DDR mostly idle while the fill crawls). DDRPERFM has NO per-master
// filter (RM0457 15.5: only rank/bank/bank-group), so this counts ALL masters;
// in this baremetal example the A35 is parked on WFE during submit_and_wait, so
// the traffic captured around a fill is effectively the GPU's.
//
// The decisive number is DDR-busy permille = (write+read events) / TCNT, which
// is calibration-free: ~a few % => idle => upstream throttle; ~90%+ => real
// DDR wall. Absolute MB/s is a cross-check against the known fill size.
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

// Clear (implicit in START) and start counting.
void ddr_start();

// Stop and snapshot the counters.
DdrSample ddr_stop();

// Pretty-print one sample with derived busy% / MB/s and a bound-vs-not verdict.
// expected_bytes: the known transfer size for the bracketed op (0 = unknown).
void ddr_report(const char *label, const DdrSample &s, uint64_t expected_bytes);
} // namespace perfmon
