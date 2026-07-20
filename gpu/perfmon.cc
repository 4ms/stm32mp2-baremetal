#include "perfmon.hh"
#include "print/print.hh"
#include "stm32mp2xx.h"

namespace perfmon
{
namespace
{
// ---- DDRPERFM register bits (RM0457 15.5 / STM32MP25_CA35.svd) ----
constexpr uint32_t CTRL_START = 1u << 0; // starts (should clear, but doesn't -- see ddr_start)
constexpr uint32_t CTRL_STOP = 1u << 1;	 // freezes all counters

// CLR (0x0C): write-1-to-clear. Bits [7:0] = per-counter, bit 8 = TCNT. Ignored
// while BUSY, so only write it after STOP. Empirically CTRL.START does NOT clear
// the counters (RM wording notwithstanding), so we clear explicitly here.
constexpr uint32_t CLR_ALL = 0x1FF;

constexpr uint32_t DRAMINF_DDR4 = 0x2; // DRAM_TYPE[1:0]: DFI decode = DDR4

// CFG0: SEL_EVENTn are 6-bit fields at bit 0/8/16/24 (n = 0..3).
constexpr uint32_t sel_event(unsigned n, uint32_t code)
{
	return (code & 0x3F) << (n * 8);
}

// SEL_EVENT codes -- RM0457 15.4.1, Table 65 (performance-logging interface,
// unambiguous per-command totals from the DDRCTRL).
constexpr uint32_t EV_OP_IS_WR = 34;  // every DRAM write command (one BL8)
constexpr uint32_t EV_OP_IS_RD = 33;  // every DRAM read command (one BL8)
constexpr uint32_t EV_OP_IS_ACT = 32; // every row activate
constexpr uint32_t EV_OP_IS_PRE = 42; // every precharge

// This DDR4 is 2x16-bit (32-bit bus), fixed BL8 => one RD/WR *command* moves
// 8 beats x 4 B = 32 bytes. So each write-command event == 32 bytes.
constexpr uint32_t BYTES_PER_EVENT = 32;

// TCNT increments every 4 DFI_CLK. Measured empirically against the 64 MHz ARM
// generic timer over the same window (tcnt=1972092 vs 827255 ARM ticks @64MHz =
// 12.93 ms) => TCNT runs at ~150 MHz. (wr+rd)/TCNT stays a valid idle indicator;
// this constant only scales the absolute MB/s cross-check.
constexpr uint32_t TCNT_HZ = 150'000'000;
} // namespace

void ddr_init()
{
	auto *p = DDRPERFM;
	p->CTRL = CTRL_STOP;	   // program only while stopped
	p->DRAMINF = DRAMINF_DDR4; // DFI decode type
	p->CFG0 =
		sel_event(0, EV_OP_IS_WR) | sel_event(1, EV_OP_IS_RD) | sel_event(2, EV_OP_IS_ACT) | sel_event(3, EV_OP_IS_PRE);
	p->CFG1 = 0;
	p->CFG2 = 0; // FILT_POL* = 000 -> no rank/bank filtering (count all)
	p->CFG3 = 0;
	p->CFG4 = 0xFFFFFFFF; // TIME_OUT = max (don't auto-stop on a long fill)
	p->CFG5 = 0x0F;		  // EVCNT_EN: enable counters 0..3 (set while stopped)
}

void ddr_start()
{
	auto *p = DDRPERFM;
	p->CTRL = CTRL_STOP; // ensure stopped (BUSY=0) so CLR is honored
	p->CLR = CLR_ALL;	 // explicitly zero all counters + TCNT
	p->CTRL = CTRL_START;
}

DdrSample ddr_stop()
{
	auto *p = DDRPERFM;
	p->CTRL = CTRL_STOP;
	return DdrSample{
		.writes = p->EVCNT0,
		.reads = p->EVCNT1,
		.acts = p->EVCNT2,
		.pres = p->EVCNT3,
		.tcnt = p->TCNT,
		.status = p->STATUS,
	};
}

void ddr_report(const char *label, const DdrSample &s, uint64_t expected_bytes)
{
	uint64_t wr_bytes = (uint64_t)s.writes * BYTES_PER_EVENT;
	uint64_t rd_bytes = (uint64_t)s.reads * BYTES_PER_EVENT;

	// DDR-busy in permille: fraction of burst-slots that carried a transfer.
	uint32_t busy_pm = s.tcnt ? (uint32_t)(((uint64_t)s.writes + s.reads) * 1000 / s.tcnt) : 0;

	// Absolute write bandwidth from the DDR-side clock (cross-check only).
	uint64_t us = (uint64_t)s.tcnt * 1'000'000 / TCNT_HZ;
	uint32_t wr_mbps = us ? (uint32_t)(wr_bytes / us) : 0;

	print(
		"  [DDRPERFM] ", label, ": wr=", s.writes, " rd=", s.reads, " act=", s.acts, " pre=", s.pres, " tcnt=", s.tcnt);
	if (s.status & 0xFF)
		print(" OVERFLOW(status=0x", Hex{s.status}, ")");
	print("\n    write ", (uint32_t)(wr_bytes >> 10), " KiB, ");
	print("read ", (uint32_t)(rd_bytes >> 10), " KiB, ");
	print("DDR-busy ", busy_pm / 10, ".", busy_pm % 10, "% , ");
	print("wr ~", wr_mbps, " MB/s\n");

	// Row locality: writes per row-activate. High = good page-mode streaming;
	// ~1 means every write reopens a row (locality broken -> that's the throttle).
	if (s.acts) {
		uint32_t wr_per_act = s.writes / s.acts;
		print("    ", wr_per_act, " writes/activate (", s.acts);
		print(" ACT, ", s.pres, " PRE) -- high is good; ~1 => row-thrashing\n");
	}

	if (expected_bytes) {
		// Did we see roughly the traffic we expected? Reveals the event unit if
		// the BYTES_PER_EVENT guess is wrong (ratio will be 8x or 32x off).
		uint32_t pct = (uint32_t)(wr_bytes * 100 / expected_bytes);
		print("    expected ~", (uint32_t)(expected_bytes >> 10), " KiB written; measured/expected = ", pct, "%\n");
	}

	print("    verdict: ");
	if (busy_pm < 250)
		print("DDR mostly IDLE -> latency/serialization-bound (throttle is UPSTREAM of DDR)\n");
	else if (busy_pm > 750)
		print("DDR SATURATED -> genuine DDR bandwidth wall\n");
	else
		print("partial -> DDR neither idle nor saturated; look at burst size / outstanding\n");
}
} // namespace perfmon
