#include "aarch64/system_reg.hh"
#include "cube_cpu_render.hh"
#include "cube_scene.hh"
#include "display.hh"
#include "drivers/hal_cnt.hh"
#include "etna.hh"
#include "etna_3d.hh"
#include "ltdc.hh"
#include "panel_etml0700z9.hh"
#include "print/print.hh"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

namespace
{
using namespace Panel;						// HActive=1024, VActive=600
constexpr uint32_t CW = 512, CH = 512;		// cube window (square -> cube stays cubic)
constexpr uint32_t CX = (HActive - CW) / 2; // 256
constexpr uint32_t CY = (VActive - CH) / 2; // 44

constexpr uint32_t pw = (CW + 15) & ~15u;
constexpr uint32_t ph = (CH + 3) & ~3u;
constexpr uint32_t RtStride = pw * 4;
constexpr uint32_t RtSize = RtStride * ph;
constexpr uint32_t DepthStride = pw * 2;
constexpr uint32_t DepthSize = DepthStride * ph;
constexpr uint32_t CubeFbSize = CW * CH * 4; // linear layer-2 framebuffer
constexpr uint32_t Background = 0xFF101828;	 // the cube's clear AND the LTDC surround

} // namespace

int main()
{
	print("\nGPU -> LTDC demo: composited spinning cube\n");
	print("==========================================\n\n");

	SystemA35_SYSTICK_Config(0);

	etna::Gpu gpu;
	if (!gpu.init()) {
		print("FAILED: GPU init\n");
		while (true)
			asm volatile("wfe");
	}

	etna::Bo rt = gpu.alloc(RtSize);	   // tiled render target (cube window)
	etna::Bo depth = gpu.alloc(DepthSize); // D16
	std::array<etna::Bo, 2> cube_fbs = {gpu.alloc(CubeFbSize), gpu.alloc(CubeFbSize)}; // display double buffer
	etna::Bo vtx = gpu.alloc(sizeof(kCubeVerts));
	etna::Bo vs = gpu.alloc(sizeof(kCubeVs));
	etna::Bo ps = gpu.alloc(sizeof(kCubeFs));
	if (!rt || !depth || !cube_fbs[0] || !cube_fbs[1] || !vtx || !vs || !ps) {
		print("FAILED: buffer alloc\n");
		while (true)
			asm volatile("wfe");
	}

	std::ranges::copy(kCubeVerts, vtx.span<float>().begin());
	vtx.cpu_fini(etna::RelocWrite);
	std::ranges::copy(kCubeVs, vs.span<uint32_t>().begin());
	vs.cpu_fini(etna::RelocWrite);
	std::ranges::copy(kCubeFs, ps.span<uint32_t>().begin());
	ps.cpu_fini(etna::RelocWrite);

	for (auto &fb : cube_fbs) {
		std::ranges::fill(fb.span<uint32_t>(), Background);
		fb.cpu_fini(etna::RelocWrite);
	}

	// Command streams are allocated once and reused every frame (reset()) because
	// the GPU pool is a bump allocator with no free.
	auto csc = gpu.new_cmd_stream(256);
	auto csd = gpu.new_cmd_stream(1024);
	auto csr = gpu.new_cmd_stream(256);

	// One frame of the cube: clear -> draw -> resolve, three separate barriers.
	auto render_frame = [&](const Mat4 &m, etna::Bo &target) -> bool {
		csc.reset();
		etna::clear(csc, rt, pw, ph, Background);
		etna::clear(csc, depth, pw, DepthSize / (pw * 4), 0xFFFFFFFF); // D16 "far"
		if (!gpu.submit_and_wait(csc))
			return false;

		csd.reset();
		etna::MeshDraw d{
			.rt = &rt,
			.rt_stride = RtStride,
			.vtx = &vtx,
			.vtx_stride = 28,
			.vs = &vs,
			.vs_words = kCubeVs.size(),
			.vs_temps = 4,
			.ps = &ps,
			.ps_words = kCubeFs.size(),
			.ps_temps = 2,
			.ps_out_reg = 1,
			.uniforms = m,
			.width = CW,
			.height = CH,
			.vertex_count = 36,
			.depth = &depth,
			.depth_stride = DepthStride,
		};
		etna::emit_mesh(csd, d);
		if (!gpu.submit_and_wait(csd))
			return false;

		csr.reset();
		etna::resolve(csr, target, rt, CW, CH, RtStride, CW * 4);
		return gpu.submit_and_wait(csr);
	};

	// Sanity-check one frame against the CPU reference renderer before we spin:
	// every pixel outside the ~1.25px edge band must match exactly. Confirms the
	// GPU transform + depth + resolve agree with the reference (0 mismatches).
	{
		etna::Bo cimg = gpu.alloc(CubeFbSize), cband = gpu.alloc(CW * CH), czbuf = gpu.alloc(CW * CH * 4);
		if (cimg && cband && czbuf) {
			constexpr float A = 0.9f;
			Mat4 vm = cube_mvp(A, 0.5f * tsin(A * 0.7f), 1.0f);
			cpu_render_cube(vm, CW, CH, cimg.span<uint32_t>(), cband.span<uint8_t>(), czbuf.span<float>(), Background);
			if (render_frame(vm, cube_fbs[1])) {
				cube_fbs[1].cpu_prep(etna::RelocRead);
				auto g = cube_fbs[1].span<const uint32_t>();
				auto c = cimg.span<const uint32_t>();
				auto b = cband.span<const uint8_t>();
				uint32_t mm = 0;
				for (uint32_t i = 0; i < CW * CH; i++)
					if (!b[i] && g[i] != c[i])
						mm++;
				print("verify: ", mm, " mismatches vs CPU reference", mm == 0 ? "  \\o/\n" : "\n");
			}
		}
	}

	// Single windowed layer: the CW x CH cube at (CX, CY), Background fills around it.
	if (!display_init(cube_fbs[0].gpu_addr(), CX, CY, CW, CH, Background)) {
		print("FAILED: LVDS PLL never locked\n");
		while (true)
			asm volatile("wfe");
	}
	print("Display up: cube ", CW, "x", CH, " at ", CX, ",", CY, "\n");

	print("Spinning...\n");
	float angle = 0.0f;
	uint32_t cur_fb = 1;
	uint32_t frames = 0;
	auto t0 = read_cntpct();
	const uint32_t tick_khz = read_cntfreq() / 1000;
	uint32_t fps = 0;
	uint32_t worst_render_tm = 0;

	auto animate_frame = [&] {
		angle += 2 * kPi / 240.0f;
		if (angle > 20 * kPi)
			angle -= 20 * kPi;
		Mat4 m = cube_mvp(angle, 0.5f * tsin(angle * 0.7f), 1.0f);

		auto render_tm_start = read_cntpct();
		if (!render_frame(m, cube_fbs[cur_fb])) {
			gpu.dump_status("cube frame");
			return;
		}
		auto render_tm_end = read_cntpct();
		worst_render_tm = std::max<uint32_t>(worst_render_tm, (render_tm_end - render_tm_start) * 1000 / tick_khz);

		ltdc_set_framebuffer(cube_fbs[cur_fb].gpu_addr());

		// Toggle buffer (double-buffered)
		cur_fb ^= 1;
	};

	std::atomic<bool> frame_ready{};

	ltdc_set_callback([&] { frame_ready.store(true, std::memory_order_release); });

	while (true) {
		if (frame_ready.load(std::memory_order_acquire)) {
			frame_ready.store(false, std::memory_order_release);
			animate_frame();

			if (++frames % 300 == 0) {
				auto now = read_cntpct();
				uint32_t us = ((now - t0) * 1000 / 300 / tick_khz);
				fps = us ? 1000000 / us : 0;
				t0 = now;

				print(fps, " fps, worst render time: ", worst_render_tm, " us = ");
				print(((fps * worst_render_tm + 5'000) / 10'000), "%\n");
				worst_render_tm = 0;
			}
		}
	}
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", int(line), "\n");
}
