#include "aarch64/system_reg.hh" // read_cntpct, read_cntfreq
#include "cube_cpu_render.hh"	 // ../gpu: CPU reference renderer (verification)
#include "cube_scene.hh"		 // ../gpu: shaders, geometry, matrices (CPU-verified)
#include "display.hh"			 // ../ltdc: board display ladder
#include "drivers/hal_cnt.hh"	 // SystemA35_SYSTICK_Config
#include "etna.hh"				 // ../gpu: GPU API
#include "etna_3d.hh"			 // ../gpu: emit_mesh / MeshDraw
#include "ltdc.hh"				 // ../ltdc: framebuffer flip + layer 2
#include "panel_etml0700z9.hh"	 // ../ltdc: panel dimensions
#include "print/print.hh"
#include <algorithm>
#include <array>
#include <cstdint>

// =============================================================================
//  gpu-ltdc-demo -- spinning cube on the LVDS panel
// =============================================================================
//
// The cube is rendered on LTDC layer 2, which is a small square window in the
// center displayed over a static background layer. Each frame the GPU only
// touches the small window's bytes (not the whole 1024x600 screen).
//
//   layer 1 (static bg 1024x600)  ---.
//                                     >-- LTDC scanout compositor --> LVDS
//   layer 2 (cube, CWxCH window)  ---'
//
// Per frame the cube goes through the GPU in three steps -- clear, draw
// (depth-tested), resolve (untile the tiled RT into the linear layer-2 buffer).
// Then the LTDC flips layer 2 at vblank. The matrix transform runs in the
// vertex shader.

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
constexpr uint32_t BgFbSize = HActive * VActive * 4;
constexpr uint32_t Background = 0xFF101828; // layer 1 AND the cube's own clear

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
	std::array<etna::Bo, 2> cube_fbs = {gpu.alloc(CubeFbSize), gpu.alloc(CubeFbSize)}; // layer-2 double buffer
	etna::Bo bg_fb = gpu.alloc(BgFbSize);											   // layer-1 static background
	etna::Bo vtx = gpu.alloc(sizeof(kCubeVerts));
	etna::Bo vs = gpu.alloc(sizeof(kCubeVs));
	etna::Bo ps = gpu.alloc(sizeof(kCubeFs));
	if (!rt || !depth || !cube_fbs[0] || !cube_fbs[1] || !bg_fb || !vtx || !vs || !ps) {
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

	std::ranges::fill(bg_fb.span<uint32_t>(), Background); // painted once, never again
	bg_fb.cpu_fini(etna::RelocWrite);
	for (auto &fb : cube_fbs) {
		std::ranges::fill(fb.span<uint32_t>(), Background);
		fb.cpu_fini(etna::RelocWrite);
	}

	// Command streams are allocated ONCE and reused every frame (reset()). The GPU
	// pool is a bump allocator with no free, so allocating fresh streams per frame
	// leaks ~6 KB/frame and exhausts the 64 MB pool in ~3 min. submit_and_wait
	// completes each op before the next, so reusing the backing Bo is safe.
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

	if (!display_init(bg_fb.gpu_addr())) { // layer 1 = the static background
		print("FAILED: LVDS PLL never locked\n");
		while (true)
			asm volatile("wfe");
	}
	ltdc_layer2_init(cube_fbs[0].gpu_addr(), CX, CY, CW, CH); // layer 2 = the cube window
	print("Display up: bg on layer 1, cube on layer 2 (", CW, "x", CH, " at ", CX, ",", CY, ")\n");

	print("spinning...\n");
	float angle = 0.0f;
	uint32_t back = 1; // cube_fbs[0] is being scanned
	uint32_t frames = 0;
	auto t0 = read_cntpct();
	const uint32_t tick_khz = static_cast<uint32_t>(read_cntfreq() / 1000);
	bool pending_fps = false;
	uint32_t fps = 0;

	while (true) {
		angle += 2 * kPi / 240.0f;
		// The motion repeats every 20*pi (y-spin period 2*pi, wobble period
		// 2*pi/0.7 -> LCM = 20*pi = 10 turns); wrap there so `angle` stays small.
		if (angle > 20 * kPi)
			angle -= 20 * kPi;
		Mat4 m = cube_mvp(angle, 0.5f * tsin(angle * 0.7f), 1.0f);
		if (!render_frame(m, cube_fbs[back])) {
			gpu.dump_status("cube frame");
			break;
		}
		ltdc_layer2_set_framebuffer(cube_fbs[back].gpu_addr());

		// Emit the fps line AFTER arming the flip, BEFORE ltdc_wait_reload, so the
		// blocking (~7 ms) UART print overlaps the idle vblank wait instead of
		// stealing time from a frame (which otherwise dropped a frame each print).
		if (pending_fps) {
			print(fps, " fps\n");
			pending_fps = false;
		}

		ltdc_wait_reload(); // vsync: blocks until the panel vblank (tear-free flip)
		back ^= 1;

		if (++frames % 300 == 0) {
			auto now = read_cntpct();
			uint32_t us = static_cast<uint32_t>((now - t0) * 1000 / 300 / tick_khz);
			fps = us ? 1000000 / us : 0;
			pending_fps = true; // printed next iteration, hidden under the vblank wait
			t0 = now;
		}
	}

	while (true)
		asm volatile("wfe");
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", int(line), "\n");
}
