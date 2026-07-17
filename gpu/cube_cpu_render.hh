#pragma once
#include "cube_scene.hh"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

// =============================================================================
//  cube_cpu_render.hh -- the CPU reference renderer for the cube scene
// =============================================================================
// Pure math, no hardware dependencies: used on-target by etna_3d_tests.cc to
// verify the GPU pixel-exactly, and on a host machine by tools/cube_preview.cc
// to render the very same scene to image files for human eyes. Keeping it one
// implementation means "what we previewed" is exactly "what the GPU matched".

// distance from p to the SEGMENT a->b (for the edge band)
inline float seg_dist(float ax, float ay, float bx, float by, float px, float py)
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
inline void cpu_render_cube(const Mat4 &m,
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
