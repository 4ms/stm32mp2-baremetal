#include "memclock_sweep.hh"
#include "aarch64/system_reg.hh" // read_cntpct / read_cntfreq
#include "drivers/hal_cnt.hh"	 // udelay
#include "drivers/rcc.hh"
#include "drivers/rcc_pll.hh"
#include "gpu_regs.hh"	// cmd_wait
#include "print/print.hh"
#include "stm32mp2xx.h" // RCC

namespace
{
uint32_t elapsed_us(uint64_t dt, uint32_t fq)
{
	return dt ? (uint32_t)(dt * 1'000'000 / fq) : 0;
}

// Submit a command stream of `k` FE WAITs (65535 cycles each) and return the
// wall-clock microseconds it took. The FE waits k*65535 core-clock cycles.
uint32_t time_fe_waits(etna::Gpu &g, uint32_t k, uint32_t fq)
{
	auto cs = g.new_cmd_stream(2 * k + 64);
	for (uint32_t i = 0; i < k; i++) {
		cs.emit(VivanteGpu::cmd_wait(0xFFFF)); // WAIT 65535 core-clock cycles
		cs.emit(0);							   // pad to the 64-bit boundary
	}
	uint64_t t0 = read_cntpct();
	g.submit_and_wait(cs, 3'000'000);
	return elapsed_us(read_cntpct() - t0, fq);
}
} // namespace

void print_gpu_clock_regs()
{
	using namespace RCC_Clocks;
	auto s3 = get_pll_settings<PLL3>();
	auto s4 = get_pll_settings<PLL4>();
	auto hz = [](auto &s) -> uint32_t {
		uint32_t ref = s.src == MuxSelSource::hse ? 40'000'000u : s.src == MuxSelSource::hsi ? 64'000'000u : 0u;
		return ref ? s.calc_freq(ref) : 0;
	};
	uint32_t pll3 = hz(s3);
	uint32_t pll4 = hz(s4);

	// flexgen ch 59 = ck_icn_m_gpu (bus). Compute its real output.
	uint32_t xbar = RCC->XBARxCFGR[59];
	uint32_t sel = xbar & 0xF;			   // source select (6 = PLL4 per HAL)
	uint32_t prediv = RCC->PREDIVxCFGR[59] & 0x3FF;
	uint32_t findiv = (RCC->FINDIVxCFGR[59] & 0x3F) + 1;
	uint32_t predivd = prediv == 0 ? 1 : prediv == 1 ? 2 : prediv == 3 ? 4 : prediv + 1;
	uint32_t flex59 = pll4 / predivd / findiv;

	// Are the PLL3 output stages actually engaged? A locked PLL whose post-divider
	// output is disabled (or bypassed) delivers the reference, not the 800 MHz.
	uint32_t c1 = RCC->PLL3CFGR1, c4 = RCC->PLL3CFGR4;
	bool pllen = c1 & (1u << 8);   // PLLEN
	bool pllrdy = c1 & (1u << 24); // PLLRDY (lock)
	bool foutpost = c4 & (1u << 9);	 // FOUTPOSTDIVEN: output + post-div enable
	bool bypass = c4 & (1u << 10); // BYPASS
	print("GPU clock regs: PLL3(ck_ker_gpu)=", pll3 / 1'000'000, " MHz (computed), PLL4=", pll4 / 1'000'000, " MHz\n");
	print("  PLL3CFGR1=0x", Hex{c1}, " CFGR4=0x", Hex{c4}, " | PLLEN=", pllen ? 1 : 0, " PLLRDY=", pllrdy ? 1 : 0,
		  " FOUTPOSTDIVEN=", foutpost ? 1 : 0, " BYPASS=", bypass ? 1 : 0);
	if (!foutpost || bypass)
		print("  <-- OUTPUT NOT ENGAGED: ck_ker_gpu is NOT the 800 MHz!");
	print("\n");
	print("  flexgen59(ck_icn_m_gpu): XBAR=0x", Hex{xbar}, " sel=", sel, " prediv=", prediv, " findiv=", findiv,
		  " -> ~", flex59 / 1'000'000, " MHz\n");
	print("  GPUCFGR=0x", Hex{RCC->GPUCFGR}, "  (en/rst/lp bits)\n");
}

uint32_t measure_gpu_core_clock_mhz(etna::Gpu &g)
{
	uint32_t fq = read_cntfreq();
	uint32_t us1 = time_fe_waits(g, 200, fq);
	uint32_t us2 = time_fe_waits(g, 1000, fq);
	// (1000-200) batches * 65535 cycles took (us2-us1) microseconds.
	// freq[MHz] = cycles / microseconds.
	uint64_t cycles = (uint64_t)(1000 - 200) * 0xFFFF;
	uint32_t mhz = (us2 > us1) ? (uint32_t)(cycles / (us2 - us1)) : 0;
	print("GPU core clock (FE-WAIT timing): ", mhz, " MHz  [200 waits=", us1, "us, 1000 waits=", us2, "us]\n");
	return mhz;
}

void memclock_sweep(etna::Gpu &g, const etna::Bo &fb, uint32_t restore_hz)
{
	constexpr uint32_t FW = 512, FH = 512;			// RS fill pixels
	constexpr uint32_t CW = 256, CH = 256;			// compute bytes
	constexpr uint64_t FBytes = (uint64_t)FW * FH * 4;

	etna::Kernel add = etna::make_kernel(g, ppu::build_add_shader);
	etna::Bo cin = g.alloc(CW * CH), cout = g.alloc(CW * CH);
	if (!add || !cin || !cout) {
		print("memclock_sweep: alloc failed\n");
		return;
	}
	for (auto p = cin.span<uint8_t>(); auto &v : p)
		v = 0x5A;
	cin.cpu_fini(etna::RelocWrite);

	uint32_t fq = read_cntfreq();
	print("\nBus-clock (ck_icn_m_gpu / flexgen59) sweep -- does GPU throughput track it?\n");
	print("  target  achieved | RS-fill us  MB/s | shader-add us\n");

	static const uint32_t kTargets[] = {600, 400, 300, 200, 150, 100, 64};
	for (uint32_t mhz : kTargets) {
		uint32_t achieved = etna::set_gpu_mem_clock(mhz * 1'000'000);
		udelay(200); // let the new clock settle
		if (!achieved) {
			print("  ", mhz, " MHz: set failed\n");
			continue;
		}

		auto cs = g.new_cmd_stream();
		etna::clear(cs, fb, FW, FH, 0x4D5A11AC);
		uint64_t t0 = read_cntpct();
		bool ok1 = g.submit_and_wait(cs, 2'000'000);
		uint32_t fill_us = elapsed_us(read_cntpct() - t0, fq);

		uint64_t t1 = read_cntpct();
		bool ok2 = etna::compute(g, add, cout, cin, CW, CH);
		uint32_t comp_us = elapsed_us(read_cntpct() - t1, fq);

		uint32_t fill_mbps = fill_us ? (uint32_t)(FBytes / fill_us) : 0;
		print("  ", mhz, " MHz  ", achieved / 1'000'000, " MHz | ", fill_us, " us ", fill_mbps, " MB/s | ", comp_us,
			  " us");
		if (!ok1 || !ok2)
			print(" (TIMEOUT)");
		print("\n");
	}

	etna::set_gpu_mem_clock(restore_hz);
	print("  (restored bus clock to ~", restore_hz / 1'000'000, " MHz)\n");
}
