#include "fscale_sweep.hh"
#include "aarch64/system_reg.hh" // read_cntpct / read_cntfreq
#include "drivers/hal_cnt.hh"	 // udelay
#include "gpu_io.hh"			 // gpu_read/gpu_write + VivanteGpu regs
#include "print/print.hh"

using namespace VivanteGpu;

namespace
{
// Load a new FSCALE_VAL into HI_CLOCK_CONTROL, same CMD_LOAD sequence the reset
// path uses: write the field with FSCALE_CMD_LOAD set, then again without it.
void set_fscale(uint32_t val)
{
	uint32_t c = gpu_read(HI_CLOCK_CONTROL) & ~0x1FCu;
	c |= CLK_FSCALE_VAL(val);
	gpu_write(HI_CLOCK_CONTROL, c | CLK_FSCALE_CMD_LOAD);
	udelay(1);
	gpu_write(HI_CLOCK_CONTROL, c);
	udelay(10); // let the new clock settle
}

uint32_t elapsed_us(uint64_t dt, uint32_t fq)
{
	return dt ? (uint32_t)(dt * 1'000'000 / fq) : 0;
}
} // namespace

void fscale_sweep(etna::Gpu &g, const etna::Bo &fb, uint32_t restore)
{
	// Two workloads per FSCALE value:
	//  - RS fill (fixed-function resolve engine, fed by the FE)
	//  - shader compute add (out=in+in on the programmable PPU/ALU cores)
	// Sizes chosen so even 1/64 clock stays well under the 2 s wait timeout.
	constexpr uint32_t FW = 512, FH = 512;			// RS fill pixels
	constexpr uint32_t CW = 256, CH = 256;			// compute bytes (u8 image)
	constexpr uint64_t FBytes = (uint64_t)FW * FH * 4;

	etna::Kernel add = etna::make_kernel(g, ppu::build_add_shader);
	etna::Bo cin = g.alloc(CW * CH), cout = g.alloc(CW * CH);
	if (!add || !cin || !cout) {
		print("fscale_sweep: kernel/buffer alloc failed\n");
		return;
	}
	for (auto p = cin.span<uint8_t>(); auto &v : p)
		v = 0x5A;
	cin.cpu_fini(etna::RelocWrite);

	uint32_t fq = read_cntfreq();
	print("\nFSCALE sweep -- RS fill (", FW, "x", FH, ") vs shader add (", CW, "x", CH, "):\n");
	print("  fscale rb | RS-fill us  MB/s | shader-add us | shader/RS speed ratio\n");

	static const uint32_t kVals[] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7F};
	uint32_t fill0_us = 0, comp0_us = 0; // 0x00 baselines for the ratio column
	for (uint32_t v : kVals) {
		set_fscale(v);
		uint32_t rb = (gpu_read(HI_CLOCK_CONTROL) >> 2) & 0x7F;

		// RS fill
		auto cs = g.new_cmd_stream();
		etna::clear(cs, fb, FW, FH, 0x4D5A11AC);
		uint64_t t0 = read_cntpct();
		bool ok1 = g.submit_and_wait(cs, 2'000'000);
		uint32_t fill_us = elapsed_us(read_cntpct() - t0, fq);

		// Shader compute add
		uint64_t t1 = read_cntpct();
		bool ok2 = etna::compute(g, add, cout, cin, CW, CH);
		uint32_t comp_us = elapsed_us(read_cntpct() - t1, fq);

		if (v == 0x00) {
			fill0_us = fill_us ? fill_us : 1;
			comp0_us = comp_us ? comp_us : 1;
		}
		uint32_t fill_mbps = fill_us ? (uint32_t)(FBytes / fill_us) : 0;
		// How much each engine slowed vs its own 0x00 baseline (x10 for 1 decimal).
		uint32_t fill_slow = fill0_us ? fill_us * 10 / fill0_us : 0;
		uint32_t comp_slow = comp0_us ? comp_us * 10 / comp0_us : 0;

		print("  0x", Hex{v}, " 0x", Hex{rb}, " | ", fill_us, " us ", fill_mbps, " MB/s | ", comp_us,
			  " us | fill ", fill_slow / 10, ".", fill_slow % 10, "x  shader ", comp_slow / 10, ".",
			  comp_slow % 10, "x slower");
		if (!ok1 || !ok2)
			print(" (TIMEOUT)");
		print("\n");
	}

	set_fscale(restore);
	print("  (restored FSCALE=0x", Hex{restore}, "; slowdowns are vs each engine's own 0x00)\n");
}
