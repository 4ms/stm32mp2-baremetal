#include "aarch64/system_reg.hh"
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

// =============================================================================
//  gpu-ltdc-demo -- N spinning cubes bouncing around in a shared 3D scene
// =============================================================================
// All cubes are rendered into ONE full-screen tiled render target with ONE
// shared depth buffer, so the depth test resolves occlusion between them (cubes
// pass in front of / behind each other by their z). The whole RT is then RS-
// resolved (untiled) to the framebuffer the LTDC scans out. Each cube has a
// world position (px,py,pz) + velocity and its own spin rate/size(via z).
// Double-buffered; vblank is interrupt-driven (ltdc callback).

namespace
{
using namespace Panel; // HActive=1024, VActive=600
constexpr uint32_t FbStride = HActive * 4;
constexpr uint32_t FbSize = FbStride * VActive;
constexpr uint32_t Background = 0xFF101828;

// Full-screen tiled render target + D16 depth (shared by every cube).
constexpr uint32_t rtpw = (HActive + 15) & ~15u; // 1024
constexpr uint32_t rtph = (VActive + 3) & ~3u;	 // 600
constexpr uint32_t RtStride = rtpw * 4;
constexpr uint32_t RtSize = RtStride * rtph;
constexpr uint32_t DepthStride = rtpw * 2;
constexpr uint32_t DepthSize = DepthStride * rtph;

constexpr float Aspect = float(HActive) / float(VActive);
constexpr float BX = 2.3f, BY = 1.3f; // world bounce bounds (keep cubes on-screen)

constexpr uint32_t NCubes = 20;
struct Cube {
	float px, py, pz;		   // world position (pz negative = into the screen)
	float vx, vy;			   // world velocity per frame
	float angle, rate;		   // spin
	float tilt_amp, tilt_freq; // X-axis wobble
};
} // namespace

int main()
{
	print("\nGPU -> LTDC demo: ", NCubes, " bouncing cubes\n");
	print("==================================\n\n");

	SystemA35_SYSTICK_Config(0);

	etna::Gpu gpu;
	if (!gpu.init()) {
		print("FAILED: GPU init\n");
		while (true)
			asm volatile("wfe");
	}

	etna::Bo rt = gpu.alloc(RtSize);
	etna::Bo depth = gpu.alloc(DepthSize);
	std::array<etna::Bo, 2> fbs = {gpu.alloc(FbSize), gpu.alloc(FbSize)}; // full-screen double buffer
	std::array<etna::Bo, NCubes> vtxs;									  // per-cube colored geometry
	for (auto &b : vtxs)
		b = gpu.alloc(sizeof(kCubeVerts));
	etna::Bo vs = gpu.alloc(sizeof(kCubeVs));
	etna::Bo ps = gpu.alloc(sizeof(kCubeFs));
	if (!rt || !depth || !fbs[0] || !fbs[1] || !vtxs[NCubes - 1] || !vs || !ps) {
		print("FAILED: buffer alloc\n");
		while (true)
			asm volatile("wfe");
	}

	std::ranges::copy(kCubeVs, vs.span<uint32_t>().begin());
	vs.cpu_fini(etna::RelocWrite);
	std::ranges::copy(kCubeFs, ps.span<uint32_t>().begin());
	ps.cpu_fini(etna::RelocWrite);

	// Per-cube geometry: each cube a distinct base hue, its 6 faces shaded so the
	// 3D structure still reads (faces are the same hue at different brightness).
	static const std::array<std::array<float, 3>, 10> baseColor = {{
		{1.0f, 0.35f, 0.35f}, // red
		{0.4f, 0.9f, 0.4f},	  // green
		{0.4f, 0.55f, 1.0f},  // blue
		{1.0f, 0.85f, 0.3f},  // amber
		{1.0f, 0.45f, 0.9f},  // pink
		{0.4f, 0.95f, 0.95f}, // cyan
		{1.0f, 0.6f, 0.25f},  // orange
		{0.7f, 0.5f, 1.0f},	  // violet
		{0.5f, 1.0f, 0.7f},	  // mint
		{0.85f, 0.85f, 0.95f} // white
	}};
	static const float faceShade[6] = {1.0f, 0.78f, 0.6f, 0.9f, 0.68f, 0.5f};
	for (uint32_t i = 0; i < NCubes; i++) {
		std::array<std::array<float, 4>, 6> fc;
		for (unsigned f = 0; f < 6; f++)
			fc[f] = {baseColor[i % 10][0] * faceShade[f],
					 baseColor[i % 10][1] * faceShade[f],
					 baseColor[i % 10][2] * faceShade[f],
					 1.0f};
		std::ranges::copy(cube_verts(fc), vtxs[i].span<float>().begin());
		vtxs[i].cpu_fini(etna::RelocWrite);
	}

	// Paint both buffers once so the pre-first-flip scan isn't garbage (the whole
	// RT is resolved every frame, so nothing here leaks into the animation).
	for (auto &fb : fbs) {
		std::ranges::fill(fb.span<uint32_t>(), Background);
		fb.cpu_fini(etna::RelocWrite);
	}

	// Deterministic spread of positions, depths, velocities, and spin rates.
	Cube cubes[NCubes];
	for (uint32_t i = 0; i < NCubes; i++) {
		float fi = float(i);
		cubes[i].px = BX * tsin(fi * 1.7f + 0.5f);
		cubes[i].py = BY * tsin(fi * 2.3f + 1.0f);
		cubes[i].pz = -2.0f - 0.4f * float(i); // -4.0 .. -6.0: varied depth
		cubes[i].vx = (0.020f + 0.004f * float(i % 3)) * ((i & 1) ? 1.f : -1.f);
		cubes[i].vy = (0.028f + 0.005f * float(i % 2)) * ((i & 2) ? 1.f : -1.f);
		cubes[i].angle = fi * 0.6f;
		cubes[i].rate = (0.2f + 0.4f * float(i % 10)) * (2 * kPi / 240.0f);
		cubes[i].tilt_amp = 0.20f + 0.12f * float(i % 4); // 0.20 .. 0.56 rad
		cubes[i].tilt_freq = 0.6f + 0.18f * float(i % 5); // 0.6 .. 1.32
	}

	auto cs = gpu.new_cmd_stream(256);	 // clear / resolve
	auto csd = gpu.new_cmd_stream(1024); // per-cube draw

	// Render the whole scene into `fb`: clear the shared RT+depth, draw every cube
	// (depth-tested against each other), then resolve the full RT to the fb.
	auto render_scene = [&](etna::Bo &fb) -> bool {
		cs.reset();
		etna::clear(cs, rt, rtpw, rtph, Background);
		etna::clear(cs, depth, rtpw, DepthSize / (rtpw * 4), 0xFFFFFFFF); // D16 far
		if (!gpu.submit_and_wait(cs))
			return false;

		for (uint32_t i = 0; i < NCubes; i++) {
			const Cube &cb = cubes[i];
			csd.reset();
			Mat4 m = cube_mvp(cb.angle, cb.tilt_amp * tsin(cb.angle * cb.tilt_freq), Aspect, cb.px, cb.py, cb.pz);
			etna::MeshDraw d{
				.rt = &rt,
				.rt_stride = RtStride,
				.vtx = &vtxs[i],
				.vtx_stride = 28,
				.vs = &vs,
				.vs_words = kCubeVs.size(),
				.vs_temps = 4,
				.ps = &ps,
				.ps_words = kCubeFs.size(),
				.ps_temps = 2,
				.ps_out_reg = 1,
				.uniforms = m,
				.width = HActive,
				.height = VActive,
				.vertex_count = 36,
				.depth = &depth,
				.depth_stride = DepthStride,
			};
			etna::emit_mesh(csd, d);
			if (!gpu.submit_and_wait(csd))
				return false;
		}

		cs.reset();
		etna::resolve(cs, fb, rt, HActive, VActive, RtStride, FbStride);
		return gpu.submit_and_wait(cs);
	};

	auto move_cubes = [&] {
		for (auto &cb : cubes) {
			cb.px += cb.vx;
			if (cb.px > BX || cb.px < -BX)
				cb.vx = -cb.vx;
			cb.py += cb.vy;
			if (cb.py > BY || cb.py < -BY)
				cb.vy = -cb.vy;
			cb.angle += cb.rate;
			if (cb.angle > 20 * kPi)
				cb.angle -= 20 * kPi;
		}
	};

	if (!display_init(fbs[0].gpu_addr())) { // full-screen single layer
		print("FAILED: LVDS PLL never locked\n");
		while (true)
			asm volatile("wfe");
	}
	print("Display up: ", NCubes, " cubes\n");
	print("Spinning...\n");

	uint32_t cur = 1; // fbs[0] is being scanned; render into fbs[1] first
	uint32_t frames = 0;
	auto t0 = read_cntpct();
	const uint32_t tick_khz = read_cntfreq() / 1000;
	uint32_t worst_us = 0;

	std::atomic<bool> frame_ready{};
	ltdc_set_callback([&] { frame_ready.store(true, std::memory_order_release); });

	while (true) {
		if (!frame_ready.load(std::memory_order_acquire))
			continue;
		frame_ready.store(false, std::memory_order_release);

		auto r0 = read_cntpct();
		if (!render_scene(fbs[cur])) {
			gpu.dump_status("scene");
			break;
		}
		worst_us = std::max<uint32_t>(worst_us, (read_cntpct() - r0) * 1000 / tick_khz);

		ltdc_set_framebuffer(fbs[cur].gpu_addr()); // vblank-latched flip
		cur ^= 1;
		move_cubes();

		if (++frames % 120 == 0) {
			auto now = read_cntpct();
			uint32_t us = (now - t0) * 1000 / 120 / tick_khz;
			print(us ? 1000000 / us : 0, " fps, worst render ", worst_us, " us\n");
			t0 = now;
			worst_us = 0;
		}
	}

	while (true)
		asm volatile("wfe");
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", int(line), "\n");
}
