#include "aarch64/system_reg.hh" // read_cntpct, read_cntfreq
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
	print("Display up (", W, "x", H, "), GPU up -- spinning...\n");

	float angle = 0.0f;
	uint32_t back = 1; // fbs[0] is being scanned
	uint32_t frames = 0;
	auto t0 = read_cntpct();
	const uint32_t tick_khz = static_cast<uint32_t>(read_cntfreq() / 1000);

	while (true) {
		angle += 2 * kPi / 240.0f; // one revolution every 4 s at 60 fps
		Mat4 m = cube_mvp(angle, 0.5f * tsin(angle * 0.7f), float(W) / float(H));

		// One submission per frame: clear color + clear depth + draw + resolve.
		auto cs = gpu.new_cmd_stream(2048);
		etna::clear(cs, rt, pw, ph, Background);
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
		etna::resolve(cs, fbs[back], rt, W, H, RtStride, W * 4);
		if (!gpu.submit_and_wait(cs)) {
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
