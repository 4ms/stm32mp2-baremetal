#include "aarch64/system_reg.hh" // read_cntpct
#include "etna.hh"
#include "etna_3d.hh"
#include "print/print.hh"
#include <algorithm>
#include <array>

using namespace VivanteGpu;
using namespace etna;

static bool verify_shape(std::span<const uint32_t> img,
						 uint32_t w,
						 uint32_t h,
						 std::span<const float, 6> ndc,
						 uint32_t inside,
						 uint32_t outside);

// Trivial HALTI5 shaders (Vivante ISA, 1 instruction = 4 dwords each).
// VS: MOV t1, t0  -- pass the position attribute (lands in t0) straight through
//     as the clip-space position; vs_pos_out_reg = 1.
constexpr std::array<uint32_t, 4> kVsCode = {0x07811009, 0x00000000, 0x00000000, 0x00390008};
// FS: MOV t1, u0  -- output the constant color from uniform u0; ps_color_out = 1.
constexpr std::array<uint32_t, 4> kPsCode = {0x07811009, 0x00000000, 0x00000000, 0x20390008};

// Per-vertex-color shaders. Same MOV encoding, decoded from the two above:
//   opcode MOV = 0x09 (word0[5:0]); DST_REG = word0[22:16]; DST_COMPS=0xF (xyzw);
//   MOV reads SRC2: SRC2_REG = word3[12:4], SRC2_RGROUP = word3[30:28]
//   (0 = temp, 2 = uniform -- that lone bit is the only diff between kVsCode and
//   kPsCode above). So word0 = 0x07801009 | (dst<<16), word3 = 0x00390008 |
//   (src<<4) [| 0x20000000 for a uniform].
// VS: pass position (in t0 -> out t2) and color (in t1 -> out t3, a varying).
constexpr std::array<uint32_t, 8> kVsColorCode = {
	// clang-format off
	0x07821009, 0x00000000, 0x00000000, 0x00390008, // MOV t2, t0  (position)
	0x07831009, 0x00000000, 0x00000000, 0x00390018, // MOV t3, t1  (color -> varying)
	// clang-format on
};
// FS: output the interpolated color varying (rasterizer lands it in t1); out t1.
constexpr std::array<uint32_t, 4> kPsColorCode = {0x07811009, 0x00000000, 0x00000000, 0x00390018}; // MOV t1, t1

// Texture-sampling FS: TEXLD t2, tex0, t1 -- sample sampler 0 at the UV varying
// (interpolated into t1, .xy used) and write RGBA to t2; ps_color_out_reg = 2.
// TEXLD = opcode 0x18 (isa: pattern 011000, OPCODE_BIT6=0). Encoding verified
// against the isa bitset "#instruction-tex":
//   word0 = 0x18 | DST_USE(1<<12) | DST_REG<<16 | COMPS(0xF<<23) | TEX_ID<<27
//   word1 = TEX_SWIZ(0xE4)<<3 | SRC0_USE(1<<11) | SRC0_REG<<12 | SRC0_SWIZ(0xE4)<<22
//   words 2,3 = 0 (no src1/src2; FS TEXLD needs no explicit LOD -- implicit
//   derivatives, and MIP=NONE/MAXLOD=0 pins level 0)
constexpr std::array<uint32_t, 4> kPsTexCode = {0x07821018, 0x39001F20, 0x00000000, 0x00000000};

// Basic drawing test: clear a small tiled RT to blue, draw a screen-covering red triangle,
// read the RT back directly (solid color -> tiling-invariant) and report how
// many pixels changed from clear -> triangle color.
// v2 (appended): RS-resolve the tiled RT to a LINEAR buffer and verify the
// triangle's actual SHAPE -- pixel positions, not just counts.
bool triangle_test(Gpu &gpu)
{
	constexpr uint32_t W = 64, H = 64;
	constexpr uint32_t pw = (W + 15) & ~15u; // pad width to 16
	constexpr uint32_t ph = (H + 3) & ~3u;	 // pad height to 4
	constexpr uint32_t stride = pw * 4;
	constexpr uint32_t rt_size = stride * ph;
	constexpr uint32_t CLEAR = 0xFF0000FF; // blue (B,G,R,A = FF,00,00,FF)

	Bo rt = gpu.alloc(rt_size);
	Bo vtx = gpu.alloc(3 * 3 * 4); // 3 verts x vec3 float
	Bo vs = gpu.alloc(sizeof(kVsCode));
	Bo ps = gpu.alloc(sizeof(kPsCode));
	if (!rt || !vtx || !vs || !ps)
		return false;

	// clear RT (solid -> same in every byte regardless of tiling)
	auto rp = rt.span<uint32_t>();
	std::ranges::fill(rp, CLEAR);
	rt.cpu_fini(RelocWrite);

	// a triangle fully INSIDE the clip volume (no fullscreen/guardband trick),
	// z = 0.5 (mid depth range, off the near plane). Covers most of the screen.
	const std::array<float, 9> verts = {-0.8f, -0.8f, 0.5f, 0.8f, -0.8f, 0.5f, 0.0f, 0.8f, 0.5f};
	std::ranges::copy(verts, vtx.span<float>().begin());
	vtx.cpu_fini(RelocWrite);

	std::ranges::copy(kVsCode, vs.span<uint32_t>().begin());
	vs.cpu_fini(RelocWrite);

	std::ranges::copy(kPsCode, ps.span<uint32_t>().begin());
	ps.cpu_fini(RelocWrite);

	const std::array<float, 4> red = {1.0f, 0.0f, 0.0f, 1.0f};

	auto cs = gpu.new_cmd_stream(1024);
	emit_triangle(cs, rt, stride, vtx, 12, vs, ps, W, H, red, 3);

	auto start = read_cntpct();
	if (!gpu.submit_and_wait(cs)) {
		gpu.dump_status("triangle draw");
		return false;
	}
	print("3D triangle drawn in ", (read_cntpct() - start), " ticks\n");

	rt.cpu_prep(RelocRead);
	// The triangle is a solid color, so it reads back the same in the tiled RT
	// regardless of layout. Count changed pixels and confirm they're one uniform
	// non-clear color (= the FS constant reached the render target).
	uint32_t drawn = 0, sample = 0;
	bool uniform = true;
	for (uint32_t i = 0; i < rt_size / 4; i++) {
		uint32_t p = rp[i];
		if (p != CLEAR) {
			if (!drawn)
				sample = p;
			else if (p != sample)
				uniform = false;
			drawn++;
		}
	}
	print("RT: ", drawn, " of ", int(rt_size / 4), " pixels drawn, color 0x", Hex{sample});
	print(uniform ? " (uniform)\n" : " (NOT uniform!)\n");
	if (drawn == 0) {
		print("FAILED: the draw wrote no pixels (pipe/state issue)\n");
		return false;
	}
	if (!uniform || sample == 0) {
		print("FAILED: triangle pixels are not a single non-zero color\n");
		return false;
	}
	print("GPU drew a solid triangle in 0x", Hex{sample}, " -- 3D pipe verified. \\o/\n");

	// --- v2: RS resolve (untile), then verify the SHAPE ----------------------
	// The resolve makes pixel (x,y) addressable at y*W + x, so we can finally
	// check WHERE the fragments landed: strictly inside the expected window-
	// space triangle must be the draw color, strictly outside must be clear.
	Bo lin = gpu.alloc(W * H * 4);
	if (!lin)
		return false;
	std::ranges::fill(lin.span<uint32_t>(), 0xDEADBEEFu); // poison: resolve must overwrite
	lin.cpu_fini(RelocWrite);

	auto cs2 = gpu.new_cmd_stream(256);
	resolve(cs2, lin, rt, W, H, stride, W * 4);
	auto rstart = read_cntpct();
	if (!gpu.submit_and_wait(cs2)) {
		gpu.dump_status("RS resolve");
		return false;
	}
	print("RS resolve (untile ", W, "x", H, ") in ", (uint32_t)(read_cntpct() - rstart), " ticks\n");

	lin.cpu_prep(RelocRead);
	const std::array<float, 6> ndc_xy = {verts[0], verts[1], verts[3], verts[4], verts[6], verts[7]};
	if (!verify_shape(lin.span<uint32_t>(), W, H, ndc_xy, sample, CLEAR))
		return false;
	print("resolved image matches the expected triangle -- shape verified. \\o/\n");
	return true;
}

// Draw an RGB-gradient triangle (red/green/blue vertices) and confirm the
// varying interpolated: the RT is tiled, but "did a red-dominant, a
// green-dominant, and a blue-dominant fragment all appear?" is tiling-invariant,
// so we can verify per-vertex interpolation without an RS untile.
bool triangle_color_test(Gpu &gpu)
{
	constexpr uint32_t W = 64, H = 64;
	constexpr uint32_t pw = (W + 15) & ~15u;
	constexpr uint32_t ph = (H + 3) & ~3u;
	constexpr uint32_t stride = pw * 4;
	constexpr uint32_t rt_size = stride * ph;
	// Clear to opaque black. The gradient's colors all satisfy R+G+B ~= 255
	// (barycentric blend of the three corners), so no drawn pixel is black --
	// black is a safe "not drawn" sentinel (unlike blue, which collides with a
	// corner color).
	constexpr uint32_t CLEAR = 0xFF000000;

	Bo rt = gpu.alloc(rt_size);
	Bo vtx = gpu.alloc(3 * 7 * 4); // 3 verts x (vec3 pos + vec4 color)
	Bo vs = gpu.alloc(sizeof(kVsColorCode));
	Bo ps = gpu.alloc(sizeof(kPsColorCode));
	if (!rt || !vtx || !vs || !ps)
		return false;

	auto rp = rt.span<uint32_t>();
	std::ranges::fill(rp, CLEAR);
	rt.cpu_fini(RelocWrite);

	// interleaved pos.xyz + color.rgba, 7 floats/vertex: red, green, blue corners
	const std::array<float, 3 * 7> verts = {
		-0.8f, -0.8f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, // v0 red
		0.8f,  -0.8f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, // v1 green
		0.0f,  0.8f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, // v2 blue
	};
	std::ranges::copy(verts, vtx.span<float>().begin());
	vtx.cpu_fini(RelocWrite);

	std::ranges::copy(kVsColorCode, vs.span<uint32_t>().begin());
	vs.cpu_fini(RelocWrite);

	std::ranges::copy(kPsColorCode, ps.span<uint32_t>().begin());
	ps.cpu_fini(RelocWrite);

	auto cs = gpu.new_cmd_stream(1024);
	emit_triangle_color(cs, rt, stride, vtx, 28, vs, ps, W, H, 3);

	auto start = read_cntpct();
	if (!gpu.submit_and_wait(cs)) {
		gpu.dump_status("triangle color draw");
		return false;
	}
	print("gradient triangle drawn in ", (uint32_t)(read_cntpct() - start), " ticks\n");

	rt.cpu_prep(RelocRead);
	uint32_t drawn = 0, first = 0;
	bool uniform = true, sawR = false, sawG = false, sawB = false;
	for (uint32_t i = 0; i < rt_size / 4; i++) {
		uint32_t p = rp[i];
		if (p == CLEAR)
			continue;
		if (!drawn)
			first = p;
		else if (p != first)
			uniform = false;
		drawn++;
		uint32_t R = (p >> 16) & 0xFF, G = (p >> 8) & 0xFF, B = p & 0xFF;
		if (R > 100 && R > G && R > B)
			sawR = true;
		if (G > 100 && G > R && G > B)
			sawG = true;
		if (B > 100 && B > R && B > G)
			sawB = true;
	}
	print("RT: ", drawn, " of ", int(rt_size / 4), " pixels drawn.");
	print(" corners seen R=", sawR, " G=", sawG, " B=", sawB, uniform ? " (uniform!)\n" : " (varied)\n");
	if (drawn == 0) {
		print("FAILED: the draw wrote no pixels\n");
		return false;
	}
	if (uniform) {
		print("FAILED: triangle is a single color -- varying did not interpolate\n");
		return false;
	}
	if (!(sawR && sawG && sawB)) {
		print("FAILED: not all three vertex colors reached fragments\n");
		return false;
	}
	print("GPU interpolated a per-vertex-color varying across the triangle. \\o/\n");
	return true;
}

// =============================================================================
//  Depth test: the nearer triangle occludes the farther one
// =============================================================================
//
// Draw the SAME triangle twice into one RT sharing a D16 depth buffer, in ONE
// command stream (so the PE's depth cache stays hot and draw 2 sees draw 1's
// depth): near (z=0.3) in RED first, then far (z=0.7) in GREEN. Window depth =
// 0.5*z + 0.5, so near->0.65, far->0.85. Depth test LESS + write: the red near
// triangle writes 0.65; the green far triangle's 0.85 fails LESS *everywhere the
// red drew* (same geometry) -> rejected. Result: the triangle stays RED.
//
// This is an unambiguous pass/fail: with depth OFF it would be painter's order
// (green drawn last wins) -> all GREEN. Solid colors -> tiling-invariant, so we
// read the tiled RT directly and just count red vs green fragments.
bool triangle_depth_test(Gpu &gpu)
{
	constexpr uint32_t W = 64, H = 64;
	constexpr uint32_t pw = (W + 15) & ~15u;
	constexpr uint32_t ph = (H + 3) & ~3u;
	constexpr uint32_t stride = pw * 4;
	constexpr uint32_t rt_size = stride * ph;
	constexpr uint32_t dstride = pw * 2; // D16 = 2 bytes/pixel, same 16-wide tiling
	constexpr uint32_t depth_size = dstride * ph;
	constexpr uint32_t CLEAR = 0xFF0000FF; // blue background

	Bo rt = gpu.alloc(rt_size);
	Bo depth = gpu.alloc(depth_size);
	Bo vnear = gpu.alloc(3 * 3 * 4);
	Bo vfar = gpu.alloc(3 * 3 * 4);
	Bo vs = gpu.alloc(sizeof(kVsCode));
	Bo ps = gpu.alloc(sizeof(kPsCode));
	if (!rt || !depth || !vnear || !vfar || !vs || !ps)
		return false;

	// Clear color to blue and depth to 0xFFFF (far). Both uniform => the tiled
	// layout doesn't matter for the clear, and the GPU addresses depth via
	// PE_DEPTH_STRIDE consistently for both draws.
	std::ranges::fill(rt.span<uint32_t>(), CLEAR);
	rt.cpu_fini(RelocWrite);
	std::ranges::fill(depth.span<uint16_t>(), uint16_t(0xFFFF));
	depth.cpu_fini(RelocWrite);

	// Red triangle in front (z=0.3) and blocking 50% of green triangle in back (z=0.7)
	// The green triangle is the same as the red triangle but flipped over the X-axis.
	const std::array<float, 9> near_v = {-0.8f, -0.8f, 0.3f, 0.8f, -0.8f, 0.3f, 0.0f, 0.8f, 0.3f};
	const std::array<float, 9> far_v = {-0.8f, 0.8f, 0.7f, 0.8f, 0.8f, 0.7f, 0.0f, -0.8f, 0.7f};
	std::ranges::copy(near_v, vnear.span<float>().begin());
	vnear.cpu_fini(RelocWrite);
	std::ranges::copy(far_v, vfar.span<float>().begin());
	vfar.cpu_fini(RelocWrite);

	std::ranges::copy(kVsCode, vs.span<uint32_t>().begin());
	vs.cpu_fini(RelocWrite);
	std::ranges::copy(kPsCode, ps.span<uint32_t>().begin());
	ps.cpu_fini(RelocWrite);

	const std::array<float, 4> red = {1.0f, 0.0f, 0.0f, 1.0f};
	const std::array<float, 4> green = {0.0f, 1.0f, 0.0f, 1.0f};

	// Both draws in one stream: near red, then far green, against the shared depth.
	auto cs = gpu.new_cmd_stream(2048);
	emit_triangle(cs, rt, stride, vnear, 12, vs, ps, W, H, red, 3, &depth, dstride);
	emit_triangle(cs, rt, stride, vfar, 12, vs, ps, W, H, green, 3, &depth, dstride);

	auto start = read_cntpct();
	if (!gpu.submit_and_wait(cs)) {
		gpu.dump_status("depth draw");
		return false;
	}
	print("depth: two triangles drawn in ", (read_cntpct() - start), " ticks\n");

	rt.cpu_prep(RelocRead);

	uint32_t drawn = 0, reds = 0, greens = 0;
	for (uint32_t p : rt.span<uint32_t>()) {
		if (p == CLEAR)
			continue;
		drawn++;
		uint32_t R = (p >> 16) & 0xFF, G = (p >> 8) & 0xFF;
		if (R > 128 && G < 64)
			reds++;
		else if (G > 128 && R < 64)
			greens++;
	}
	print("depth test: ", drawn, " drawn -> ", reds, " red (near), ", greens, " green (far)\n");
	if (drawn == 0) {
		print("FAILED: nothing drawn\n");
		return false;
	}
	if (greens > (reds / 2 + 6) || greens < (reds / 2 - 6)) { // +/-6 in case we have an aliased pixel each intersection
		print("FAILED: wrong number of green fragments survived -> we should see ~50% green");
		return false;
	}
	if (reds == 0) {
		print("FAILED: no red -> near triangle missing\n");
		return false;
	}
	print("GPU depth test occluded the farther triangle -- depth buffer works. \\o/\n");
	return true;
}

// Texture test: a 64x64 tiled A8R8G8B8 texture with four solid quadrants
// (TL red, TR green, BL blue, BR white), sampled by the usual triangle with UVs
// (0,0) (1,0) (0.5,1) -- which covers all four UV quadrants. NEAREST filtering
// returns exact texel values, so every drawn pixel must be one of the four
// quadrant colors, and (tiling-invariantly) all four must appear.
bool triangle_texture_test(Gpu &gpu)
{
	constexpr uint32_t W = 64, H = 64;	 // render target
	constexpr uint32_t TW = 64, TH = 64; // texture (pow2)
	constexpr uint32_t pw = (W + 15) & ~15u;
	constexpr uint32_t ph = (H + 3) & ~3u;
	constexpr uint32_t stride = pw * 4;
	constexpr uint32_t rt_size = stride * ph;
	constexpr uint32_t tpw = (TW + 15) & ~15u; // texture padded width
	constexpr uint32_t tstride = tpw * 4;
	constexpr uint32_t tex_size = tstride * ((TH + 3) & ~3u);
	constexpr uint32_t CLEAR = 0xFF000000; // black; no quadrant color is black
	constexpr std::array<uint32_t, 4> QUAD = {
		0xFFFF0000, // TL red
		0xFF00FF00, // TR green
		0xFF0000FF, // BL blue
		0xFFFFFFFF, // BR white
	};

	Bo rt = gpu.alloc(rt_size);
	Bo tex = gpu.alloc(tex_size); // 64B aligned (alloc default)
	Bo desc = gpu.alloc(256);	  // TXDESC: 256 B, 64B aligned
	Bo vtx = gpu.alloc(3 * 7 * 4);
	Bo vs = gpu.alloc(sizeof(kVsColorCode));
	Bo ps = gpu.alloc(sizeof(kPsTexCode));
	if (!rt || !tex || !desc || !vtx || !vs || !ps)
		return false;

	std::ranges::fill(rt.span<uint32_t>(), CLEAR);
	rt.cpu_fini(RelocWrite);

	// Linear (x, y) -> 32-bit-word index in the basic-tiled texture layout the
	// sampler reads (4x4-pixel tiles, row-major; Mesa etnaviv_tiling.c).
	constexpr auto tiled_index = [](uint32_t x, uint32_t y, uint32_t stride_px) -> uint32_t {
		return (y / 4) * (stride_px * 4) + (y % 4) * 4 + (x / 4) * 16 + (x % 4);
	};

	// Texel data, written directly in the basic-tiled layout the sampler reads.
	auto tp = tex.span<uint32_t>();
	for (uint32_t y = 0; y < TH; y++)
		for (uint32_t x = 0; x < TW; x++)
			tp[tiled_index(x, y, tpw)] = QUAD[(y < TH / 2 ? 0 : 2) + (x < TW / 2 ? 0 : 1)];
	tex.cpu_fini(RelocWrite);

	fill_tex_descriptor(desc.span<uint32_t>(), tex.gpu_addr(), TW, TH, tstride);
	desc.cpu_fini(RelocWrite);

	// pos vec3 + UV as vec4 (u, v, 0, 1) -- same layout/stride as the color draw
	const std::array<float, 3 * 7> verts = {
		-0.8f, -0.8f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, // v0: UV (0,0)
		0.8f,  -0.8f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, // v1: UV (1,0)
		0.0f,  0.8f,  0.5f, 0.5f, 1.0f, 0.0f, 1.0f, // v2: UV (0.5,1)
	};
	std::ranges::copy(verts, vtx.span<float>().begin());
	vtx.cpu_fini(RelocWrite);

	std::ranges::copy(kVsColorCode, vs.span<uint32_t>().begin()); // same VS: pass pos + vec4 varying
	vs.cpu_fini(RelocWrite);
	std::ranges::copy(kPsTexCode, ps.span<uint32_t>().begin());
	ps.cpu_fini(RelocWrite);

	auto cs = gpu.new_cmd_stream(1024);
	emit_triangle_tex(cs, rt, stride, vtx, 28, vs, ps, desc, W, H, 3);

	auto start = read_cntpct();
	if (!gpu.submit_and_wait(cs)) {
		gpu.dump_status("textured draw");
		return false;
	}
	print("textured triangle drawn in ", (uint32_t)(read_cntpct() - start), " ticks\n");

	rt.cpu_prep(RelocRead);
	uint32_t drawn = 0, other = 0;
	std::array<uint32_t, 4> counts{};
	for (uint32_t p : rt.span<uint32_t>()) {
		if (p == CLEAR)
			continue;
		drawn++;
		bool matched = false;
		for (uint32_t q = 0; q < 4; q++)
			if (p == QUAD[q]) {
				counts[q]++;
				matched = true;
				break;
			}
		if (!matched)
			other++;
	}
	print("texture test: ",
		  drawn,
		  " drawn -> R=",
		  counts[0],
		  " G=",
		  counts[1],
		  " B=",
		  counts[2],
		  " W=",
		  counts[3],
		  " other=",
		  other,
		  "\n");
	if (drawn == 0) {
		print("FAILED: nothing drawn\n");
		return false;
	}
	if (!(counts[0] && counts[1] && counts[2] && counts[3])) {
		print("FAILED: not all four texel quadrants were sampled\n");
		return false;
	}
	print("GPU sampled a 2D texture across the triangle -- texturing works. \\o/\n");
	return true;
}

// --- shape verification (needs a resolved / linear image) --------------------
// Signed distance (in px) from point p to the directed edge a->b. With a
// positive-winding triangle, all three edge distances are positive inside.
static float edge_dist(float ax, float ay, float bx, float by, float px, float py)
{
	float ex = bx - ax, ey = by - ay;
	return (ex * (py - ay) - ey * (px - ax)) / __builtin_sqrtf(ex * ex + ey * ey);
}

// Verify a resolved (linear, y*w + x indexed) image is exactly `inside` color
// within the window-space triangle given by NDC verts, and `outside` elsewhere.
// Pixels whose center is within 1 px of a triangle edge are ignored -- the
// rasterizer's fill rule owns the boundary. The window-Y direction (does NDC
// +Y go up or down the framebuffer rows?) isn't pinned yet, so try both and
// report which one the hardware uses.
static bool verify_shape(std::span<const uint32_t> img,
						 uint32_t w,
						 uint32_t h,
						 std::span<const float, 6> ndc, // x0,y0, x1,y1, x2,y2
						 uint32_t inside,
						 uint32_t outside)
{
	std::array<uint32_t, 2> bad{};
	for (int flip = 0; flip < 2; flip++) {
		// NDC -> window pixels via our viewport (scale/offset = w/2, h/2)
		std::array<float, 6> v;
		for (int i = 0; i < 3; i++) {
			v[i * 2 + 0] = (w / 2.0f) + (w / 2.0f) * ndc[i * 2 + 0];
			v[i * 2 + 1] = (h / 2.0f) + (h / 2.0f) * (flip ? -ndc[i * 2 + 1] : ndc[i * 2 + 1]);
		}
		// force positive winding so "inside" == all edge distances positive
		float area2 = (v[2] - v[0]) * (v[5] - v[1]) - (v[3] - v[1]) * (v[4] - v[0]);
		if (area2 < 0) {
			std::swap(v[2], v[4]);
			std::swap(v[3], v[5]);
		}
		uint32_t mismatches = 0;
		for (uint32_t y = 0; y < h; y++)
			for (uint32_t x = 0; x < w; x++) {
				float px = x + 0.5f, py = y + 0.5f;
				float d0 = edge_dist(v[0], v[1], v[2], v[3], px, py);
				float d1 = edge_dist(v[2], v[3], v[4], v[5], px, py);
				float d2 = edge_dist(v[4], v[5], v[0], v[1], px, py);
				float dmin = std::min({d0, d1, d2});
				uint32_t p = img[y * w + x];
				if (dmin > 1.0f) { // strictly inside (by > 1 px)
					if (p != inside)
						mismatches++;
				} else if (dmin < -1.0f) { // strictly outside
					if (p != outside)
						mismatches++;
				}
			}
		bad[flip] = mismatches;
		if (mismatches == 0) {
			print("shape: exact (NDC +Y = ", flip ? "decreasing" : "increasing", " framebuffer rows)\n");
			return true;
		}
	}
	print("shape FAILED: ", bad[0], " / ", bad[1], " strict mismatches (y-as-is / y-flipped)\n");
	return false;
}

// =============================================================================
//  Spinning cube: matrix transform in the VS + depth + CPU-reference verify
// =============================================================================
//
// The first multi-instruction vertex shader: clip = M * position, with the 4x4
// matrix as 4 column vec4s in VS uniforms u0..u3 (MUL + 3x MAD), plus the
// proven color varying and D16 depth test. No display yet, so verification is
// numeric: a CPU reference renderer transforms + rasterizes the same 36
// vertices with the same matrix and a float z-buffer, and every frame must
// match the resolved GPU image pixel-exactly outside a ~1.25 px band around
// projected triangle edges (fixed-point rasterization owns the edges).

// --- minimal float-ALU instruction builder -----------------------------------
// Field layout as decoded and hardware-proven for MOV/TEXLD: opcode word0[5:0],
// DST_USE bit12, DST_REG word0[22:16], COMPS word0[26:23]; per-source fields
// USE / REG[9] / (type bit) / SWIZ[8] / neg / abs / AMODE / RGROUP at
// bits 43+ (src0), 70+ (src1), 99+ (src2); rgroup 0 = temp, 2 = uniform.
struct Src {
	uint32_t reg = 0;
	uint32_t rgroup = 0;  // 0 = temp, 2 = uniform
	uint32_t swiz = 0xE4; // .xyzw; broadcasts: .xxxx=0x00 .yyyy=0x55 .zzzz=0xAA .wwww=0xFF
	bool use = true;
};
inline constexpr Src kNoSrc{.use = false};

// opcodes: MUL=0x03 (dst=s0*s1), MAD=0x02 (dst=s0*s1+s2), MOV=0x09 (dst=s2)
static constexpr std::array<uint32_t, 4> alu_inst(uint32_t opcode, uint32_t dst, Src s0, Src s1, Src s2)
{
	std::array<uint32_t, 4> w{};
	w[0] = (opcode & 0x3Fu) | (1u << 12) | (dst << 16) | (0xFu << 23);
	if (s0.use) {
		w[1] = (1u << 11) | (s0.reg << 12) | (s0.swiz << 22);
		w[2] = s0.rgroup << 3;
	}
	if (s1.use) {
		w[2] |= (1u << 6) | (s1.reg << 7) | (s1.swiz << 17);
		w[3] = s1.rgroup;
	}
	if (s2.use)
		w[3] |= (1u << 3) | (s2.reg << 4) | (s2.swiz << 14) | (s2.rgroup << 28);
	return w;
}

// The builder must reproduce the two hardware-proven MOV encodings exactly.
static_assert(alu_inst(0x09, 1, kNoSrc, kNoSrc, Src{.reg = 0, .rgroup = 0}) ==
			  std::array<uint32_t, 4>{0x07811009, 0, 0, 0x00390008});
static_assert(alu_inst(0x09, 1, kNoSrc, kNoSrc, Src{.reg = 0, .rgroup = 2}) ==
			  std::array<uint32_t, 4>{0x07811009, 0, 0, 0x20390008});

// VS: clip position = M * pos, matrix COLUMNS in u0..u3 (t0 = pos, its w = 1.0
// from the NFE vec3 default), then the color passthrough t1 -> t3 (varying).
// vs_pos_out_reg = t2, varying = t3 -- same linkage as the color triangle.
static constexpr auto kCubeVs = [] {
	std::array<uint32_t, 5 * 4> c{};
	auto put = [&](unsigned i, std::array<uint32_t, 4> w) {
		for (unsigned j = 0; j < 4; j++)
			c[i * 4 + j] = w[j];
	};
	put(0, alu_inst(0x03, 2, Src{.reg = 0, .rgroup = 2}, Src{.reg = 0, .swiz = 0x00}, kNoSrc));		 // MUL t2, u0, t0.xxxx
	put(1, alu_inst(0x02, 2, Src{.reg = 1, .rgroup = 2}, Src{.reg = 0, .swiz = 0x55}, Src{.reg = 2})); // MAD t2, u1, t0.yyyy, t2
	put(2, alu_inst(0x02, 2, Src{.reg = 2, .rgroup = 2}, Src{.reg = 0, .swiz = 0xAA}, Src{.reg = 2})); // MAD t2, u2, t0.zzzz, t2
	put(3, alu_inst(0x02, 2, Src{.reg = 3, .rgroup = 2}, Src{.reg = 0, .swiz = 0xFF}, Src{.reg = 2})); // MAD t2, u3, t0.wwww, t2
	put(4, alu_inst(0x09, 3, kNoSrc, kNoSrc, Src{.reg = 1}));										 // MOV t3, t1
	return c;
}();

// --- tiny CPU-side math -------------------------------------------------------
// Accuracy is irrelevant: the CPU reference uses the very same matrix values
// the GPU gets as uniforms -- they only need to be identical, not exact.
inline constexpr float kPi = 3.14159265f;
static float tsin(float x)
{
	while (x > kPi)
		x -= 2 * kPi;
	while (x < -kPi)
		x += 2 * kPi;
	float x2 = x * x;
	return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f)));
}
static float tcos(float x)
{
	return tsin(x + kPi / 2);
}

using Mat4 = std::array<float, 16>; // column-major: m[col*4 + row]
static Mat4 mat_mul(const Mat4 &a, const Mat4 &b)
{
	Mat4 r{};
	for (int c = 0; c < 4; c++)
		for (int row = 0; row < 4; row++) {
			float s = 0;
			for (int k = 0; k < 4; k++)
				s += a[k * 4 + row] * b[c * 4 + k];
			r[c * 4 + row] = s;
		}
	return r;
}

// Model-view-projection: rotate the cube (Y spin + X tilt), push it to z = -3,
// project with f = 2 (zn = 1, zf = 10). Cube half-size 0.5 keeps everything
// well inside the frustum -- we have not brought up the clipper.
static Mat4 cube_mvp(float angle, float tilt)
{
	float cy = tcos(angle), sy = tsin(angle);
	float cx = tcos(tilt), sx = tsin(tilt);
	const Mat4 ry = {cy, 0, -sy, 0, /**/ 0, 1, 0, 0, /**/ sy, 0, cy, 0, /**/ 0, 0, 0, 1};
	const Mat4 rx = {1, 0, 0, 0, /**/ 0, cx, sx, 0, /**/ 0, -sx, cx, 0, /**/ 0, 0, 0, 1};
	const Mat4 tr = {1, 0, 0, 0, /**/ 0, 1, 0, 0, /**/ 0, 0, 1, 0, /**/ 0, 0, -3.0f, 1};
	constexpr float f = 2.0f, zn = 1.0f, zf = 10.0f;
	const Mat4 proj = {f, 0, 0, 0, /**/ 0, f, 0, 0, /**/ 0, 0, (zf + zn) / (zn - zf), -1, /**/
					   0, 0, 2 * zf * zn / (zn - zf), 0};
	return mat_mul(proj, mat_mul(tr, mat_mul(rx, ry)));
}

// --- cube geometry: 36 verts x (pos vec3 + color vec4), solid color per face --
inline constexpr std::array<std::array<float, 4>, 6> kFaceColors = {{
	{1, 0, 0, 1}, // +X red
	{0, 1, 0, 1}, // -X green
	{0, 0, 1, 1}, // +Y blue
	{1, 1, 0, 1}, // -Y yellow
	{1, 0, 1, 1}, // +Z magenta
	{0, 1, 1, 1}, // -Z cyan
}};
static constexpr uint32_t face_argb(unsigned f)
{
	const auto &c = kFaceColors[f];
	return 0xFF000000u | (uint32_t(c[0] * 255) << 16) | (uint32_t(c[1] * 255) << 8) | uint32_t(c[2] * 255);
}
static constexpr auto kCubeVerts = [] {
	// corner index bits: bit0 -> x, bit1 -> y, bit2 -> z (0 = -0.5, 1 = +0.5)
	constexpr std::array<std::array<int, 4>, 6> quads = {{
		{1, 3, 7, 5}, // +X
		{0, 4, 6, 2}, // -X
		{2, 6, 7, 3}, // +Y
		{0, 1, 5, 4}, // -Y
		{4, 5, 7, 6}, // +Z
		{0, 2, 3, 1}, // -Z
	}};
	std::array<float, 36 * 7> v{};
	unsigned o = 0;
	for (unsigned f = 0; f < 6; f++) {
		const std::array<int, 6> idx = {quads[f][0], quads[f][1], quads[f][2],
										quads[f][0], quads[f][2], quads[f][3]};
		for (int ci : idx) {
			v[o++] = (ci & 1) ? 0.5f : -0.5f;
			v[o++] = (ci & 2) ? 0.5f : -0.5f;
			v[o++] = (ci & 4) ? 0.5f : -0.5f;
			for (unsigned k = 0; k < 4; k++)
				v[o++] = kFaceColors[f][k];
		}
	}
	return v;
}();

// distance from p to the SEGMENT a->b (for the edge band)
static float seg_dist(float ax, float ay, float bx, float by, float px, float py)
{
	float ex = bx - ax, ey = by - ay;
	float len2 = ex * ex + ey * ey;
	float t = len2 > 0 ? ((px - ax) * ex + (py - ay) * ey) / len2 : 0.0f;
	t = t < 0 ? 0 : (t > 1 ? 1 : t);
	float dx = px - (ax + t * ex), dy = py - (ay + t * ey);
	return __builtin_sqrtf(dx * dx + dy * dy);
}

// CPU reference: transform + rasterize the same 36 verts with the same matrix,
// float z-buffer, LESS, no culling -- the GPU's exact configuration. Also mark
// a ~1.25 px band around every projected edge, where the hardware's fixed-point
// rasterization may legitimately differ from our float math.
static void cpu_render_cube(const Mat4 &m,
							uint32_t w,
							uint32_t h,
							std::span<uint32_t> img,
							std::span<uint8_t> band,
							std::span<float> zbuf,
							uint32_t clear)
{
	std::array<float, 36 * 3> win; // window x, y, depth per vertex
	for (unsigned i = 0; i < 36; i++) {
		const float *p = &kCubeVerts[i * 7];
		float cx = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
		float cy = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
		float cz = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
		float cw = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
		win[i * 3 + 0] = (w / 2.0f) + (w / 2.0f) * (cx / cw);
		win[i * 3 + 1] = (h / 2.0f) + (h / 2.0f) * (cy / cw); // NDC +Y = increasing rows
		win[i * 3 + 2] = 0.5f + 0.5f * (cz / cw);			  // window depth in [0,1]
	}
	std::ranges::fill(img, clear);
	std::ranges::fill(band, uint8_t{0});
	std::ranges::fill(zbuf, 1.0f);

	auto ef = [](const float *a, const float *b, float px, float py) {
		return (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
	};
	for (unsigned t = 0; t < 12; t++) {
		const float *v0 = &win[(t * 3 + 0) * 3];
		const float *v1 = &win[(t * 3 + 1) * 3];
		const float *v2 = &win[(t * 3 + 2) * 3];
		uint32_t argb = face_argb(t / 2);

		// edge band (marked even for edge-on triangles the fill loop skips)
		for (unsigned e = 0; e < 3; e++) {
			const float *a = &win[(t * 3 + e) * 3];
			const float *b = &win[(t * 3 + (e + 1) % 3) * 3];
			int x0 = int(std::min(a[0], b[0]) - 2), x1 = int(std::max(a[0], b[0]) + 2);
			int y0 = int(std::min(a[1], b[1]) - 2), y1 = int(std::max(a[1], b[1]) + 2);
			for (int y = std::max(y0, 0); y <= std::min(y1, int(h) - 1); y++)
				for (int x = std::max(x0, 0); x <= std::min(x1, int(w) - 1); x++)
					if (seg_dist(a[0], a[1], b[0], b[1], x + 0.5f, y + 0.5f) < 1.25f)
						band[y * w + x] = 1;
		}

		float area = ef(v0, v1, v2[0], v2[1]);
		if (area > -1e-6f && area < 1e-6f)
			continue; // edge-on sliver: no fill (its band is already marked)
		int x0 = int(std::min({v0[0], v1[0], v2[0]})), x1 = int(std::max({v0[0], v1[0], v2[0]}) + 1);
		int y0 = int(std::min({v0[1], v1[1], v2[1]})), y1 = int(std::max({v0[1], v1[1], v2[1]}) + 1);
		for (int y = std::max(y0, 0); y <= std::min(y1, int(h) - 1); y++)
			for (int x = std::max(x0, 0); x <= std::min(x1, int(w) - 1); x++) {
				float px = x + 0.5f, py = y + 0.5f;
				float w0 = ef(v1, v2, px, py), w1 = ef(v2, v0, px, py), w2 = ef(v0, v1, px, py);
				bool inside = area > 0 ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0);
				if (!inside)
					continue;
				float z = (w0 * v0[2] + w1 * v1[2] + w2 * v2[2]) / area;
				if (z < zbuf[y * w + x]) {
					zbuf[y * w + x] = z;
					img[y * w + x] = argb;
				}
			}
	}
}

// Draw the cube at several rotation angles; every frame must match the CPU
// reference pixel-exactly outside the edge band, every drawn pixel must be one
// of the 6 exact face colors, and across the spin all 6 faces must appear.
bool spinning_cube_test(etna::Gpu &gpu)
{
	constexpr uint32_t W = 64, H = 64;
	constexpr uint32_t pw = (W + 15) & ~15u;
	constexpr uint32_t ph = (H + 3) & ~3u;
	constexpr uint32_t stride = pw * 4;
	constexpr uint32_t rt_size = stride * ph;
	constexpr uint32_t dstride = pw * 2;
	constexpr uint32_t depth_size = dstride * ph;
	constexpr uint32_t CLEAR = 0xFF000000;
	constexpr int FRAMES = 8;

	Bo rt = gpu.alloc(rt_size);
	Bo depthb = gpu.alloc(depth_size);
	Bo vtx = gpu.alloc(sizeof(kCubeVerts));
	Bo vsb = gpu.alloc(sizeof(kCubeVs));
	Bo psb = gpu.alloc(sizeof(kPsColorCode));
	Bo lin = gpu.alloc(W * H * 4);
	if (!rt || !depthb || !vtx || !vsb || !psb || !lin)
		return false;

	std::ranges::copy(kCubeVerts, vtx.span<float>().begin());
	vtx.cpu_fini(RelocWrite);
	std::ranges::copy(kCubeVs, vsb.span<uint32_t>().begin());
	vsb.cpu_fini(RelocWrite);
	std::ranges::copy(kPsColorCode, psb.span<uint32_t>().begin());
	psb.cpu_fini(RelocWrite);

	static std::array<uint32_t, W * H> cpu_img;
	static std::array<uint8_t, W * H> band;
	static std::array<float, W * H> zbuf;

	uint32_t colors_seen = 0;
	uint64_t gpu_ticks = 0;
	for (int frame = 0; frame < FRAMES; frame++) {
		float angle = 0.3f + frame * (2 * kPi / FRAMES);
		Mat4 m = cube_mvp(angle, 0.6f * tsin(angle));

		std::ranges::fill(rt.span<uint32_t>(), CLEAR);
		rt.cpu_fini(RelocWrite);
		std::ranges::fill(depthb.span<uint16_t>(), uint16_t(0xFFFF));
		depthb.cpu_fini(RelocWrite);

		auto cs = gpu.new_cmd_stream(1024);
		etna::MeshDraw d{
			.rt = &rt,
			.rt_stride = stride,
			.vtx = &vtx,
			.vtx_stride = 28,
			.vs = &vsb,
			.vs_words = kCubeVs.size(),
			.vs_temps = 4,
			.ps = &psb,
			.ps_words = kPsColorCode.size(),
			.ps_temps = 2,
			.ps_out_reg = 1,
			.uniforms = m,
			.width = W,
			.height = H,
			.vertex_count = 36,
			.depth = &depthb,
			.depth_stride = dstride,
		};
		etna::emit_mesh(cs, d);
		auto t0 = read_cntpct();
		if (!gpu.submit_and_wait(cs)) {
			gpu.dump_status("cube draw");
			return false;
		}
		auto cs2 = gpu.new_cmd_stream(256);
		etna::resolve(cs2, lin, rt, W, H, stride, W * 4);
		if (!gpu.submit_and_wait(cs2)) {
			gpu.dump_status("cube resolve");
			return false;
		}
		gpu_ticks += read_cntpct() - t0;
		lin.cpu_prep(RelocRead);

		cpu_render_cube(m, W, H, cpu_img, band, zbuf, CLEAR);

		uint32_t mismatches = 0, alien = 0, banded = 0, drawn = 0;
		auto gp = lin.span<const uint32_t>();
		for (uint32_t i = 0; i < W * H; i++) {
			uint32_t p = gp[i];
			if (p != CLEAR) {
				drawn++;
				bool known = false;
				for (unsigned f = 0; f < 6; f++)
					if (p == face_argb(f)) {
						colors_seen |= 1u << f;
						known = true;
						break;
					}
				if (!known)
					alien++;
			}
			if (band[i]) {
				banded++;
				continue;
			}
			if (p != cpu_img[i]) {
				if (mismatches < 3)
					print("  frame ", frame, " mismatch at (", i % W, ",", i / W, "): gpu 0x", Hex{p}, " cpu 0x",
						  Hex{cpu_img[i]}, "\n");
				mismatches++;
			}
		}
		print("cube frame ", frame, ": ", drawn, " px drawn, ", mismatches, " mismatches (", banded,
			  " edge px ignored)\n");
		if (mismatches || alien) {
			print("FAILED: frame ", frame, " -- ", mismatches, " mismatches, ", alien, " alien colors\n");
			return false;
		}
	}
	print("spinning cube: ", FRAMES, " frames avg ", uint32_t(gpu_ticks / FRAMES), " ticks (draw+resolve), faces seen 0x",
		  Hex{colors_seen}, "\n");
	if (colors_seen != 0x3F) {
		print("FAILED: not all 6 faces appeared over the spin\n");
		return false;
	}
	print("GPU spun a cube: VS matrix transform + depth + rasterization all match the CPU. \\o/\n");
	return true;
}
