#include "aarch64/system_reg.hh" // read_cntpct
#include "drivers/hal_cnt.hh"	 // SystemA35_SYSTICK_Config
#include "etna.hh"
#include "print/print.hh"
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
	auto pixels = static_cast<uint32_t *>(fb.map());
	for (uint32_t i = 0; i < ImgWidth * ImgHeight; i++)
		pixels[i] = 0xDEADBEEF; // poison so a partial fill is obvious
	fb.cpu_fini(etna::RelocWrite);

	auto cs = gpu.new_cmd_stream();
	etna::clear(cs, fb, ImgWidth, ImgHeight, ClearColor);

	auto start = read_cntpct();
	if (!gpu.submit_and_wait(cs))
		return false;
	print("RS fill (", int(ImgWidth), "x", int(ImgHeight), ") in ", (uint32_t)(read_cntpct() - start), " ticks\n");

	fb.cpu_prep(etna::RelocRead);
	for (uint32_t i = 0; i < ImgWidth * ImgHeight; i++) {
		if (pixels[i] != ClearColor) {
			print("ERROR: fill wrong at [", int(i), "] = 0x", Hex{pixels[i]}, "\n");
			return false;
		}
	}
	print("GPU filled ", int(ImgWidth), "x", int(ImgHeight), " with 0x", Hex{ClearColor}, " -- verified. \\o/\n");
	return true;
}

// Copy source -> dest through the RS engine, swapping R and B in flight, and
// verify the per-pixel transform with the CPU.
bool test_blit_convert(etna::Gpu &gpu, const etna::Bo &dst, const etna::Bo &src)
{
	auto s = static_cast<uint32_t *>(src.map());
	auto d = static_cast<uint32_t *>(dst.map());
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
	print("RS blit+convert (",
		  int(ImgWidth),
		  "x",
		  int(ImgHeight),
		  ") in ",
		  (uint32_t)(read_cntpct() - start),
		  " ticks\n");

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

} // namespace

int main()
{
	print("\nGPU Example (etna API)\n");
	print("======================\n\n");

	SystemA35_SYSTICK_Config(0); // generic timer, so udelay()/read_cntpct() work

	etna::Gpu gpu;
	bool ok = gpu.init();

	etna::Bo fb, src;
	if (ok) {
		fb = gpu.alloc(ImgWidth * ImgHeight * 4);
		src = gpu.alloc(ImgWidth * ImgHeight * 4);
		ok = fb && src;
		if (!ok)
			print("ERROR: could not allocate GPU buffers\n");
	}

	if (ok)
		ok = test_fill(gpu, fb);
	if (ok)
		ok = test_blit_convert(gpu, fb, src);
	if (ok)
		ok = test_throughput(gpu);
	if (ok)
		ok = etna::compute_test(gpu);

	print(ok ? "\nSUCCESS\n" : "\nFAILED (see above)\n");

	while (true)
		asm volatile("wfe");
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", int(line), "\n");
}
