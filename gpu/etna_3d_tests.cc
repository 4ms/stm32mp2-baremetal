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
