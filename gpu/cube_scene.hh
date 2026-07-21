#pragma once
#include <array>
#include <cstdint>

// =============================================================================
//  cube_scene.hh -- reusable spinning-cube scene: shaders, geometry, matrices
// =============================================================================
// Shared by the gpu/ test suite (etna_3d_tests.cc, CPU-verified) and the
// gpu-ltdc-demo/ (live on the LVDS panel). Header-only, everything constexpr
// where possible; the instruction builder proves itself against the two
// hardware-verified MOV encodings at compile time.

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
constexpr std::array<uint32_t, 4> alu_inst(uint32_t opcode, uint32_t dst, Src s0, Src s1, Src s2)
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

// The builder must reproduce the hardware-proven MOV encodings exactly.
static_assert(alu_inst(0x09, 1, kNoSrc, kNoSrc, Src{.reg = 0, .rgroup = 0}) ==
			  std::array<uint32_t, 4>{0x07811009, 0, 0, 0x00390008});
static_assert(alu_inst(0x09, 1, kNoSrc, kNoSrc, Src{.reg = 0, .rgroup = 2}) ==
			  std::array<uint32_t, 4>{0x07811009, 0, 0, 0x20390008});

// VS: clip position = M * pos, matrix COLUMNS in u0..u3 (t0 = pos, its w = 1.0
// from the NFE vec3 default), then the color passthrough t1 -> t3 (varying).
// vs_pos_out_reg = t2, varying = t3 -- same linkage as the color triangle.
inline constexpr auto kCubeVs = [] {
	std::array<uint32_t, 5 * 4> c{};
	auto put = [&](unsigned i, std::array<uint32_t, 4> w) {
		for (unsigned j = 0; j < 4; j++)
			c[i * 4 + j] = w[j];
	};
	put(0, alu_inst(0x03, 2, Src{.reg = 0, .rgroup = 2}, Src{.reg = 0, .swiz = 0x00}, kNoSrc)); // MUL t2, u0, t0.xxxx
	put(1, alu_inst(0x02, 2, Src{.reg = 1, .rgroup = 2}, Src{.reg = 0, .swiz = 0x55}, Src{.reg = 2})); // MAD t2, u1,
																									   // t0.yyyy, t2
	put(2, alu_inst(0x02, 2, Src{.reg = 2, .rgroup = 2}, Src{.reg = 0, .swiz = 0xAA}, Src{.reg = 2})); // MAD t2, u2,
																									   // t0.zzzz, t2
	put(3, alu_inst(0x02, 2, Src{.reg = 3, .rgroup = 2}, Src{.reg = 0, .swiz = 0xFF}, Src{.reg = 2})); // MAD t2, u3,
																									   // t0.wwww, t2
	put(4, alu_inst(0x09, 3, kNoSrc, kNoSrc, Src{.reg = 1}));										   // MOV t3, t1
	return c;
}();

// FS: output the interpolated color varying (arrives in t1); ps_color_out = 1.
// Built by the same builder; must equal the hardware-proven kPsColorCode.
inline constexpr auto kCubeFs = alu_inst(0x09, 1, kNoSrc, kNoSrc, Src{.reg = 1});
static_assert(kCubeFs == std::array<uint32_t, 4>{0x07811009, 0, 0, 0x00390018});

// --- tiny CPU-side math -------------------------------------------------------
// Accuracy is irrelevant when a CPU reference uses the very same values the
// GPU gets as uniforms -- they only need to be identical, not exact.
inline constexpr float kPi = 3.14159265f;
inline float tsin(float x)
{
	while (x > kPi)
		x -= 2 * kPi;
	while (x < -kPi)
		x += 2 * kPi;
	if (x > kPi / 2)
		x = kPi - x;
	else if (x < -kPi / 2)
		x = -kPi - x;
	float x2 = x * x;
	return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f)));
}
inline float tcos(float x)
{
	return tsin(x + kPi / 2);
}

using Mat4 = std::array<float, 16>; // column-major: m[col*4 + row]
inline Mat4 mat_mul(const Mat4 &a, const Mat4 &b)
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

// Model-view-projection: rotate the cube (Y spin + X tilt), translate it to
// world (px, py, pz) (default 0,0,-3), project with f = 2 (zn = 1, zf = 10).
// Cube half-size 0.5. `aspect` = viewport width/height so the cube stays cubic on
// non-square targets. With a shared depth buffer, cubes at different pz occlude
// each other; pz must stay inside the frustum (roughly -zn..-zf, i.e. -1..-10).
inline Mat4 cube_mvp(float angle, float tilt, float aspect = 1.0f, float px = 0.0f, float py = 0.0f, float pz = -3.0f)
{
	float cy = tcos(angle), sy = tsin(angle);
	float cx = tcos(tilt), sx = tsin(tilt);
	const Mat4 ry = {cy, 0, -sy, 0, /**/ 0, 1, 0, 0, /**/ sy, 0, cy, 0, /**/ 0, 0, 0, 1};
	const Mat4 rx = {1, 0, 0, 0, /**/ 0, cx, sx, 0, /**/ 0, -sx, cx, 0, /**/ 0, 0, 0, 1};
	const Mat4 tr = {1, 0, 0, 0, /**/ 0, 1, 0, 0, /**/ 0, 0, 1, 0, /**/ px, py, pz, 1};
	constexpr float f = 2.0f, zn = 1.0f, zf = 10.0f;
	const Mat4 proj = {f / aspect, 0, 0, 0, /**/ 0, f, 0, 0, /**/ 0, 0, (zf + zn) / (zn - zf), -1, /**/
					   0,		   0, 2 * zf * zn / (zn - zf), 0};
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
constexpr uint32_t face_argb(unsigned f)
{
	const auto &c = kFaceColors[f];
	return 0xFF000000u | (uint32_t(c[0] * 255) << 16) | (uint32_t(c[1] * 255) << 8) | uint32_t(c[2] * 255);
}
// Build the 36 vertices (2 tris x 6 faces), each face a solid `fc[f]` color.
// constexpr, but also callable at runtime to make per-instance colored cubes.
constexpr std::array<float, 36 * 7> cube_verts(const std::array<std::array<float, 4>, 6> &fc)
{
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
		const std::array<int, 6> idx = {quads[f][0], quads[f][1], quads[f][2], quads[f][0], quads[f][2], quads[f][3]};
		for (int ci : idx) {
			v[o++] = (ci & 1) ? 0.5f : -0.5f;
			v[o++] = (ci & 2) ? 0.5f : -0.5f;
			v[o++] = (ci & 4) ? 0.5f : -0.5f;
			for (unsigned k = 0; k < 4; k++)
				v[o++] = fc[f][k];
		}
	}
	return v;
}
inline constexpr auto kCubeVerts = cube_verts(kFaceColors);
