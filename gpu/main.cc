#include "aarch64/system_reg.hh" // read_cntpct
#include "drivers/hal_cnt.hh"	 // SystemA35_SYSTICK_Config
#include "etna.hh"
#include "etna_3d_tests.hh"
#include "print/print.hh"
#include "stm32mp2xx.h" // RCC (clock diagnostics)
#include <cstdint>

// GPU example, now built on the etna API (see etna.hh / etna.cc).
//
// Bring-up (power, clock, RIF, reset, identify) is etna::Gpu::init(). Work is
// issued by emitting an operation into a command stream and submitting it:
//     etna::clear(cs, dst, w, h, color);  gpu.submit_and_wait(cs);
// The two RS tests (fill, blit+convert) are now thin callers of that API.

namespace
{
constexpr uint32_t ImgWidth = 1024;
constexpr uint32_t ImgHeight = 1024;
constexpr uint32_t ClearColor = 0x4D5A11AC;

// Swap the red and blue channels of an A8R8G8B8 pixel: 0xAARRGGBB -> 0xAABBGGRR
constexpr uint32_t swap_rb(uint32_t px)
{
	return (px & 0xFF00FF00u) | ((px >> 16) & 0xFFu) | ((px & 0xFFu) << 16);
}

// Exercise the WAIT/LINK ring: run N small clears two ways and compare.
//  - sequential: submit + wait for each (one op in flight at a time)
//  - pipelined:  submit all N, then wait only for the last
// The FE is never reset between ops (it has been running the ring since
// init()); pipelining lets it run them back-to-back with no host round-trip.
bool test_throughput(etna::Gpu &gpu)
{
	constexpr int N = 16;	   // <= 29 so the rolling event ids stay distinct
	constexpr uint32_t W = 64; // small: per-op overhead, not RS bandwidth, dominates
	constexpr uint32_t H = 64;

	auto buf = gpu.alloc(W * H * 4);
	if (!buf)
		return false;

	auto t0 = read_cntpct();
	for (int i = 0; i < N; i++) {
		auto cs = gpu.new_cmd_stream();
		etna::clear(cs, buf, W, H, 0x11223300u | i);
		if (!gpu.submit_and_wait(cs))
			return false;
	}
	auto seq = (uint32_t)(read_cntpct() - t0);

	auto t1 = read_cntpct();
	etna::Fence last;
	for (int i = 0; i < N; i++) {
		auto cs = gpu.new_cmd_stream();
		etna::clear(cs, buf, W, H, 0x22334400u | i);
		last = gpu.submit(cs); // queue without waiting
	}
	if (!gpu.wait(last)) // one wait for the whole batch (FIFO -> last is last)
		return false;
	auto pipe = (uint32_t)(read_cntpct() - t1);

	print("Ring throughput (", int(N), " x ", int(W), "x", int(H), " clears): ");
	print("sequential ", seq, " ticks, pipelined ", pipe, " ticks\n");
	return true;
}

// Fill a buffer with a solid color via the RS engine, and verify with the CPU.
bool test_fill(etna::Gpu &gpu, const etna::Bo &fb)
{
	auto pixels = fb.span<uint32_t>();
	for (uint32_t i = 0; i < ImgWidth * ImgHeight; i++)
		pixels[i] = 0xDEADBEEF; // poison so a partial fill is obvious
	fb.cpu_fini(etna::RelocWrite);

	auto cs = gpu.new_cmd_stream();
	etna::clear(cs, fb, ImgWidth, ImgHeight, ClearColor);

	auto start = read_cntpct();
	if (!gpu.submit_and_wait(cs))
		return false;
	auto end = read_cntpct();
	print("RS fill (", ImgWidth, "x", ImgHeight, ") in ", (end - start), " ticks\n");

	fb.cpu_prep(etna::RelocRead);
	for (uint32_t i = 0; i < ImgWidth * ImgHeight; i++) {
		if (pixels[i] != ClearColor) {
			print("ERROR: fill wrong at [", i, "] = 0x", Hex{pixels[i]}, "\n");
			return false;
		}
	}
	print("GPU filled ", ImgWidth, "x", ImgHeight, " with 0x", Hex{ClearColor}, " -- verified. \\o/\n");
	return true;
}

// Copy source -> dest through the RS engine, swapping R and B in flight, and
// verify the per-pixel transform with the CPU.
bool test_blit_convert(etna::Gpu &gpu, const etna::Bo &dst, const etna::Bo &src)
{
	auto s = src.span<uint32_t>();
	auto d = dst.span<uint32_t>();
	for (uint32_t y = 0; y < ImgHeight; y++)
		for (uint32_t x = 0; x < ImgWidth; x++)
			s[y * ImgWidth + x] = 0xFF000000u | ((x & 0xFFu) << 16) | ((y & 0xFFu) << 8) | ((x + y) & 0xFFu);
	for (uint32_t i = 0; i < ImgWidth * ImgHeight; i++)
		d[i] = 0xDEADBEEF;
	src.cpu_fini(etna::RelocWrite);
	dst.cpu_fini(etna::RelocWrite);

	auto cs = gpu.new_cmd_stream();
	etna::blit(cs, dst, src, ImgWidth, ImgHeight, etna::Format::A8R8G8B8, etna::BlitSwapRB);

	auto start = read_cntpct();
	if (!gpu.submit_and_wait(cs))
		return false;
	auto end = read_cntpct();
	print("RS blit+convert (", ImgWidth, "x", ImgHeight, ") in ", (end - start), " ticks\n");

	dst.cpu_prep(etna::RelocRead);
	for (uint32_t i = 0; i < ImgWidth * ImgHeight; i++) {
		if (d[i] != swap_rb(s[i])) {
			print("ERROR: blit wrong at [", int(i), "] src 0x", Hex{s[i]});
			print(" expected 0x", Hex{swap_rb(s[i])}, " got 0x", Hex{d[i]}, "\n");
			return false;
		}
	}
	print("GPU copied ", int(ImgWidth), "x", int(ImgHeight), ", swapping R<->B -- verified. \\o/\n");
	return true;
}

// Build `build`, run it once over in -> out (width x height u8), return the
// elapsed ticks. For the launch-overhead diagnostic below.
uint64_t
time_kernel(etna::Gpu &gpu, etna::ShaderBuilder build, const etna::Bo &out, const etna::Bo &in, uint32_t w, uint32_t h)
{
	auto k = etna::make_kernel(gpu, build);
	if (!k)
		return 0;
	auto s = read_cntpct();
	etna::compute(gpu, k, out, in, w, h);
	return read_cntpct() - s;
}

// Real-image integration: alpha-blend two ARGB8888 images with a per-pixel
// alpha, on the programmable shader cores via the compute API. An ARGB W x H
// image is just a u8 image of (4*W) x H, so the per-byte alpha-lerp kernel
// blends each color channel independently:
//     out = mul_hi(A, alpha) + mul_hi(B, 255-alpha)   (per byte)
// A = red horizontal gradient, B = blue vertical gradient, alpha = a left->right
// ramp (same across a pixel's 4 bytes), so the result cross-fades A into B.
bool test_image_blend(etna::Gpu &gpu)
{
	constexpr uint32_t W = 512, H = 512; // ARGB pixels
	constexpr uint32_t BW = W * 4;		 // width in BYTES (the u8 image width; 2048 % 16 == 0)

	auto a = gpu.alloc(BW * H);
	auto b = gpu.alloc(BW * H);
	auto alpha = gpu.alloc(BW * H);
	auto out = gpu.alloc(BW * H);
	if (!a || !b || !alpha || !out)
		return false;

	auto ap = a.span<uint8_t>();
	auto bp = b.span<uint8_t>();
	auto lp = alpha.span<uint8_t>();
	for (uint32_t y = 0; y < H; y++)
		for (uint32_t x = 0; x < W; x++) {
			uint32_t p = (y * W + x) * 4;			   // byte offset of pixel (B,G,R,A)
			uint8_t av = uint8_t((x * 255) / (W - 1)); // alpha ramps left -> right
			ap[p + 0] = 0;
			ap[p + 1] = 0;
			ap[p + 2] = uint8_t((x * 255) / (W - 1)); // A: red gradient
			ap[p + 3] = 255;
			bp[p + 0] = uint8_t((y * 255) / (H - 1)); // B: blue gradient
			bp[p + 1] = 0;
			bp[p + 2] = 0;
			bp[p + 3] = 255;
			lp[p + 0] = lp[p + 1] = lp[p + 2] = lp[p + 3] = av; // same alpha per channel
		}
	a.cpu_fini(etna::RelocWrite);
	b.cpu_fini(etna::RelocWrite);
	alpha.cpu_fini(etna::RelocWrite);

	auto blend = etna::make_kernel(gpu, ppu::build_blend_lerp_shader);
	if (!blend)
		return false;

	auto start = read_cntpct();
	if (!etna::compute(gpu, blend, out, a, b, alpha, BW, H))
		return false;
	auto gpu_ticks = read_cntpct() - start;
	print("Alpha-blended two ", W, "x", H, " ARGB images (GPU) in ", gpu_ticks, " ticks\n");

	out.cpu_prep(etna::RelocRead);
	auto op = out.span<uint8_t>();
	for (uint32_t i = 0; i < BW * H; i++) {
		uint32_t al = lp[i];
		uint8_t expect = uint8_t(((uint32_t(ap[i]) * al) >> 8) + ((uint32_t(bp[i]) * (255 - al)) >> 8));
		if (op[i] != expect) {
			print("ERROR: image blend wrong at byte ", int(i), " got ", int(op[i]), " expected ", int(expect), "\n");
			return false;
		}
	}
	print("GPU alpha-blended ", W, "x", H, " ARGB (per-channel lerp) -- verified. \\o/\n");

	// Pure-CPU reference: the identical per-byte math on the identical buffers,
	// timed the same way, so the GPU compute path can be compared head to head.
	auto ocpu = gpu.alloc(BW * H);
	if (!ocpu)
		return false;
	auto cp = ocpu.span<uint8_t>();
	auto cstart = read_cntpct();
	for (uint32_t i = 0; i < BW * H; i++) {
		uint32_t al = lp[i];
		cp[i] = uint8_t(((uint32_t(ap[i]) * al) >> 8) + ((uint32_t(bp[i]) * (255 - al)) >> 8));
	}
	auto cpu_ticks = read_cntpct() - cstart;
	print("Same blend on the CPU in ", cpu_ticks, " ticks");
	if (cpu_ticks)
		print("   (GPU / CPU = ", (uint32_t)(gpu_ticks / cpu_ticks), "x)");
	print("\n");

	// The CPU result must match the GPU's, byte for byte.
	for (uint32_t i = 0; i < BW * H; i++)
		if (cp[i] != op[i]) {
			print("ERROR: CPU/GPU blend mismatch at byte ", int(i), "\n");
			return false;
		}

	return true;
}

// Clock-tree diagnostics for the GPU's two clocks: the core clock (ck_ker_gpu
// = PLL3, which we program to 800 MHz) and -- the suspect -- the AXI/memory
// interface clock (ck_icn_m_gpu = FLEXGEN CHANNEL 59, which our bring-up has
// never touched; whatever the boot chain left there throttles ALL GPU DDR
// traffic regardless of the core clock).
//
// Also measures the FE's actual clock empirically: a stream of N WAIT(200)
// commands takes N*200 core cycles; timing it with the 64 MHz generic timer
// gives the real core frequency, independent of what the PLL registers claim.
void clock_diag(etna::Gpu &gpu)
{
	print("\n-- GPU clock diagnostics --\n");
	print("GPUCFGR 0x", Hex{RCC->GPUCFGR}, "  PLL3CFGR1 0x", Hex{RCC->PLL3CFGR1}, "\n");
	print("flexgen 59 (ck_icn_m_gpu, GPU AXI/memory clock):\n");
	print("  XBAR59 0x", Hex{RCC->XBARxCFGR[59]}, " (src[2:0]: 0=PLL4 1=PLL5 2=PLL6 3=PLL7 4=PLL8 5=HSI 6=HSE)\n");
	print("  PREDIV59 0x", Hex{RCC->PREDIVxCFGR[59]}, "  FINDIV59 0x", Hex{RCC->FINDIVxCFGR[59]}, "\n");
	print("PLL4..8 CFGR1: 0x", Hex{RCC->PLL4CFGR1}, " 0x", Hex{RCC->PLL5CFGR1}, " 0x", Hex{RCC->PLL6CFGR1}, " 0x",
		  Hex{RCC->PLL7CFGR1}, " 0x", Hex{RCC->PLL8CFGR1}, "\n");

	// --- measure the FE/core clock via WAIT commands --------------------------
	constexpr uint32_t N = 2000; // N * 200 cycles of pure FE waiting
	auto measure = [&](bool with_waits) -> uint64_t {
		auto cs = gpu.new_cmd_stream(2 * N + 64);
		if (with_waits)
			for (uint32_t i = 0; i < N; i++) {
				cs.emit(VivanteGpu::cmd_wait(200));
				cs.emit(VivanteGpu::CMD_NOP); // WAIT occupies a qword
			}
		auto t0 = read_cntpct();
		gpu.submit_and_wait(cs);
		return read_cntpct() - t0;
	};
	uint64_t overhead = measure(false);
	uint64_t waits = measure(true);
	uint64_t dt = waits > overhead ? waits - overhead : 1; // 64 MHz ticks
	// core MHz = (N*200 cycles) / (dt / 64MHz)
	uint32_t core_mhz = static_cast<uint32_t>((uint64_t)N * 200 * 64 / dt);
	print("FE WAIT timing: ", (uint32_t)waits, " - ", (uint32_t)overhead, " ticks -> core clock ~", core_mhz,
		  " MHz (expect 800)\n\n");
}

} // namespace

int main()
{
	print("\nGPU Example (etna API)\n");
	print("======================\n\n");

	SystemA35_SYSTICK_Config(0); // generic timer, so udelay()/read_cntpct() work

	etna::Gpu gpu;
	bool ok = gpu.init();
	if (ok)
		clock_diag(gpu);

	etna::Bo fb, src;
	if (ok) {
		fb = gpu.alloc(ImgWidth * ImgHeight * 4);
		src = gpu.alloc(ImgWidth * ImgHeight * 4);
		ok = fb && src;
		if (!ok)
			print("ERROR: could not allocate GPU buffers\n");
	}

	print("\nRS Engine tests:\n");
	if (ok)
		ok = test_fill(gpu, fb);
	if (ok)
		ok = test_blit_convert(gpu, fb, src);
	if (ok)
		ok = test_throughput(gpu);

	print("\nPPU compute/shader tests:\n");
	if (ok)
		ok = etna::compute_test(gpu);
	if (ok)
		ok = test_image_blend(gpu);

	print("\n3D tests:\n");
	if (ok)
		ok = triangle_test(gpu);
	if (ok)
		ok = triangle_color_test(gpu);
	if (ok)
		ok = triangle_depth_test(gpu);
	if (ok)
		ok = triangle_texture_test(gpu);
	if (ok)
		ok = spinning_cube_test(gpu);

	print(ok ? "\nSUCCESS\n" : "\nFAILED (see above)\n");

	while (true)
		asm volatile("wfe");
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", int(line), "\n");
}
