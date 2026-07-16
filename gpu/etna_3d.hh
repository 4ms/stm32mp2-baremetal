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

} // namespace etna
