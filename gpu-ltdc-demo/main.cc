#include "aarch64/system_reg.hh" // read_cntpct, read_cntfreq
#include "cube_cpu_render.hh"	 // ../gpu: the CPU reference renderer (verification)
#include "cube_scene.hh"		 // ../gpu: shaders, geometry, matrices (CPU-verified)
#include "display.hh"			 // ../ltdc: board display ladder
#include "drivers/hal_cnt.hh"	 // SystemA35_SYSTICK_Config
#include "etna.hh"				 // ../gpu: GPU API
#include "etna_3d.hh"			 // ../gpu: emit_mesh / MeshDraw
#include "ltdc.hh"				 // ../ltdc: framebuffer flip
#include "panel_etml0700z9.hh"	 // ../ltdc: panel dimensions
#include "print/print.hh"
#include <algorithm>
#include <array>
#include <cstdint>

// =============================================================================
//  gpu-ltdc-demo -- the spinning cube, live on the LVDS panel
// =============================================================================
//
// The two bring-ups joined: per frame, the GPU clears + draws the depth-tested
// cube into a TILED render target (the only thing its pixel engine can write),
// the RS engine resolves (untiles) it into a linear back buffer, and the LTDC
// flips to it at vblank. Double-buffered, tear-free.
//
//   cube_scene.hh (verts + matrix VS)     [CPU-verified in gpu/etna_3d_tests]
//        v  emit_mesh          (3D pipe, D16 depth)
//   tiled RT 1024x600
//        v  etna::clear x2 + draw + resolve, one submit per frame
//   linear back buffer  --ltdc_set_framebuffer()-->  LTDC --> LVDS --> panel

namespace
{
constexpr uint32_t W = Panel::HActive; // 1024
constexpr uint32_t H = Panel::VActive; // 600
constexpr uint32_t pw = (W + 15) & ~15u;
constexpr uint32_t ph = (H + 3) & ~3u;
constexpr uint32_t RtStride = pw * 4;
constexpr uint32_t RtSize = RtStride * ph;
constexpr uint32_t DepthStride = pw * 2;
constexpr uint32_t DepthSize = DepthStride * ph;
constexpr uint32_t FbSize = W * H * 4;
constexpr uint32_t Background = 0xFF101828; // dark blue-gray
} // namespace

int main()
{
	print("\nGPU -> LTDC demo: spinning cube on the LVDS panel\n");
	print("=================================================\n\n");

	SystemA35_SYSTICK_Config(0);

	etna::Gpu gpu;
	if (!gpu.init()) {
		print("FAILED: GPU init\n");
		while (true)
			asm volatile("wfe");
	}

	etna::Bo rt = gpu.alloc(RtSize);	   // tiled render target
	etna::Bo depth = gpu.alloc(DepthSize); // D16
	std::array<etna::Bo, 2> fbs = {gpu.alloc(FbSize), gpu.alloc(FbSize)};
	etna::Bo vtx = gpu.alloc(sizeof(kCubeVerts));
	etna::Bo vs = gpu.alloc(sizeof(kCubeVs));
	etna::Bo ps = gpu.alloc(sizeof(kCubeFs));
	if (!rt || !depth || !fbs[0] || !fbs[1] || !vtx || !vs || !ps) {
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

	for (auto &fb : fbs) { // first scanned frames are a clean background
		std::ranges::fill(fb.span<uint32_t>(), Background);
		fb.cpu_fini(etna::RelocWrite);
	}

	if (!display_init(fbs[0].gpu_addr())) {
		print("FAILED: LVDS PLL never locked\n");
		while (true)
			asm volatile("wfe");
	}
	print("Display up (", W, "x", H, "), GPU up\n");

	// Render one animation frame into `target`: clear color + clear depth +
	// draw + resolve, one submission. `cpu_depth_clear` selects how the depth
	// buffer is cleared -- RS fill (the demo's way) or CPU fill + cache clean
	// (the way the hardware-verified 64x64 test did it). The A/B matters for
	// the verification below.
	auto render_frame = [&](const Mat4 &m, etna::Bo &target, bool cpu_depth_clear) -> bool {
		if (cpu_depth_clear) {
			std::ranges::fill(depth.span<uint16_t>(), uint16_t(0xFFFF));
			depth.cpu_fini(etna::RelocWrite);
		}
		auto cs = gpu.new_cmd_stream(2048);
		etna::clear(cs, rt, pw, ph, Background);
		if (!cpu_depth_clear)
			etna::clear(cs, depth, pw, DepthSize / (pw * 4), 0xFFFFFFFF); // D16 "far"
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
			.width = W,
			.height = H,
			.vertex_count = 36,
			.depth = &depth,
			.depth_stride = DepthStride,
		};
		etna::emit_mesh(cs, d);
		etna::resolve(cs, target, rt, W, H, RtStride, W * 4);
		return gpu.submit_and_wait(cs);
	};

	// --- one-frame GPU-vs-CPU verification at FULL panel resolution ----------
	// The 64x64 test proved GPU == CPU at aspect 1 with CPU-cleared depth and
	// one draw per submission. The panel demo runs 1024x600, aspect 1.71, an
	// RS-cleared depth, and clear+draw+resolve in one stream -- all unverified
	// deltas. Compare a frame in both depth-clear modes and categorize the
	// mismatches: "wrong-face" = occlusion broken, "missing" = fragments
	// killed, "extra" = stray writes.
	{
		etna::Bo cimg = gpu.alloc(W * H * 4);
		etna::Bo cband = gpu.alloc(W * H);
		etna::Bo czbuf = gpu.alloc(W * H * 4);
		if (cimg && cband && czbuf) {
			constexpr float VerifyAngle = 0.9f; // a nicely three-faced view
			Mat4 vm = cube_mvp(VerifyAngle, 0.5f * tsin(VerifyAngle * 0.7f), float(W) / float(H));
			cpu_render_cube(vm, W, H, cimg.span<uint32_t>(), cband.span<uint8_t>(), czbuf.span<float>(), Background);
			for (int mode = 0; mode < 2; mode++) {
				bool cpu_clear = mode == 1;
				if (!render_frame(vm, fbs[1], cpu_clear))
					break;
				fbs[1].cpu_prep(etna::RelocRead);
				auto g = fbs[1].span<const uint32_t>();
				auto c = cimg.span<const uint32_t>();
				auto b = cband.span<const uint8_t>();
				uint32_t mm = 0, extra = 0, missing = 0, wrong_face = 0;
				for (uint32_t i = 0; i < W * H; i++) {
					if (b[i] || g[i] == c[i])
						continue;
					mm++;
					if (c[i] == Background)
						extra++;
					else if (g[i] == Background)
						missing++;
					else
						wrong_face++;
					if (mm <= 4)
						print("  (", i % W, ",", i / W, "): gpu 0x", Hex{g[i]}, " cpu 0x", Hex{c[i]}, "\n");
				}
				print("verify[depth clear = ", cpu_clear ? "CPU" : "RS ", "]: ", mm, " mismatches (extra ", extra,
					  ", missing ", missing, ", wrong-face ", wrong_face, ")\n");
			}
		}
	}

	print("spinning...\n");
	float angle = 0.0f;
	uint32_t back = 1; // fbs[0] is being scanned
	uint32_t frames = 0;
	auto t0 = read_cntpct();
	const uint32_t tick_khz = static_cast<uint32_t>(read_cntfreq() / 1000);

	while (true) {
		angle += 2 * kPi / 240.0f; // one revolution every 4 s at 60 fps
		Mat4 m = cube_mvp(angle, 0.5f * tsin(angle * 0.7f), float(W) / float(H));

		if (!render_frame(m, fbs[back], false)) {
			gpu.dump_status("cube frame");
			break;
		}

		ltdc_set_framebuffer(fbs[back].gpu_addr()); // flip at next vblank
		ltdc_wait_reload();							// don't draw into the front buffer
		back ^= 1;

		if (++frames % 120 == 0) {
			auto now = read_cntpct();
			uint32_t ms_per_frame = static_cast<uint32_t>((now - t0) / 120 / tick_khz);
			print("120 frames, ", ms_per_frame, " ms/frame (~", ms_per_frame ? 1000 / ms_per_frame : 0,
				  " fps), ISR 0x", Hex{ltdc_isr()}, "\n");
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
