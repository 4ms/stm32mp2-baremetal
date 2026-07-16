#pragma once
#include "etna.hh"
#include <cstdint>
#include <span>

namespace etna
{

void fill_tex_descriptor(std::span<uint32_t> d, uint32_t tex_addr, uint32_t w, uint32_t h, uint32_t stride);

void emit_triangle(CmdStream &cs,
				   const Bo &rt,
				   uint32_t rt_stride,
				   const Bo &vtx,
				   uint32_t vtx_stride,
				   const Bo &vs,
				   const Bo &ps,
				   uint32_t width,
				   uint32_t height,
				   std::span<const float, 4> color,
				   uint32_t vertex_count,
				   const Bo *depth = nullptr,
				   uint32_t depth_stride = 0);

void emit_triangle_color(CmdStream &cs,
						 const Bo &rt,
						 uint32_t rt_stride,
						 const Bo &vtx,
						 uint32_t vtx_stride,
						 const Bo &vs,
						 const Bo &ps,
						 uint32_t width,
						 uint32_t height,
						 uint32_t vertex_count);

void emit_triangle_tex(CmdStream &cs,
					   const Bo &rt,
					   uint32_t rt_stride,
					   const Bo &vtx,
					   uint32_t vtx_stride,
					   const Bo &vs,
					   const Bo &ps,
					   const Bo &desc,
					   uint32_t width,
					   uint32_t height,
					   uint32_t vertex_count);

// Generalized mesh draw: N interleaved vertices of pos-vec3 + one vec4
// attribute (stride 28), carried to the FS as one smooth vec4 varying;
// optional float uniforms uploaded to the unified bank (VS u0.., base 0 --
// e.g. a 4x4 transform as 4 column vec4s); optional D16 LESS depth test with
// writes. Shader sizes are parametric (dwords; 4 per instruction).
struct MeshDraw {
	const Bo *rt = nullptr;
	uint32_t rt_stride = 0;
	const Bo *vtx = nullptr;
	uint32_t vtx_stride = 28;
	const Bo *vs = nullptr;
	uint32_t vs_words = 0; // VS length in dwords
	uint32_t vs_temps = 4;
	const Bo *ps = nullptr;
	uint32_t ps_words = 0;
	uint32_t ps_temps = 2;
	uint32_t ps_out_reg = 1;
	std::span<const float> uniforms{}; // empty = none
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t vertex_count = 0;
	const Bo *depth = nullptr; // optional depth buffer (cleared by caller)
	uint32_t depth_stride = 0;
};
void emit_mesh(CmdStream &cs, const MeshDraw &d);

} // namespace etna
