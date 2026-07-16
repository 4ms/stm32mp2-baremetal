#include "aarch64/system_reg.hh" // read_cntpct
#include "etna.hh"
#include "gpu_regs.hh"
#include "gpu_regs_3d.hh"
#include "print/print.hh"
#include <algorithm>
#include <array>

// =============================================================================
//  etna_3d.cc -- minimal 3D triangle on the HALTI5 graphics pipe
// =============================================================================
//
// Hand-port of the Mesa etnaviv draw path (etna_emit_state / etna_draw_vbo) for
// OUR exact chip (customer 0x15, HALTI5, 2 shader cores, 1 pixel pipe, no BLT).
// See the recipe in the session notes; registers in gpu_regs_3d.hh.
//
// v1 goal: prove the pipe runs end to end. We draw a screen-COVERING triangle in
// a solid color over a solid-color clear. Because a solid fill is tiling-
// invariant (every byte identical), we can read the TILED render target directly
// -- no RS resolve, no untile -- and just check the pixels changed from clear to
// the triangle color. Shape/tiling verification is a v2 concern.
//
// Shaders are trivial 1-instruction HALTI5 programs, uploaded to BOs and fetched
// via the shader instruction cache (ICACHE) -- HALTI5 has no inline shader load.

namespace etna
{
using namespace VivanteGpu;

// float -> raw 32-bit pattern (for fui() register values).
static uint32_t fui(float f)
{
	uint32_t u;
	__builtin_memcpy(&u, &f, sizeof(u));
	return u;
}
// float -> signed 16.16 fixed point (viewport scale/offset, scissor).
static uint32_t fixp16(float f)
{
	return static_cast<uint32_t>(static_cast<int32_t>(f * 65536.0f));
}

// LOAD_STATE with the FIXP header bit (VIV_FE_LOAD_STATE_HEADER_FIXP): the FE
// converts the 16.16 fixed-point data word to float before writing the register.
// Mandatory for the viewport scale/offset X/Y and scissor/clip registers --
// without it a fixp value lands in a float register as garbage (wrong viewport).
static void set_state_fixp(CmdStream &cs, uint32_t addr, uint32_t value)
{
	cs.emit(cmd_load_state(addr) | 0x04000000u);
	cs.emit(value);
}

// Trivial HALTI5 shaders (Vivante ISA, 1 instruction = 4 dwords each).
// VS: MOV t1, t0  -- pass the position attribute (lands in t0) straight through
//     as the clip-space position; vs_pos_out_reg = 1.
static const std::array<uint32_t, 4> kVsCode = {0x07811009, 0x00000000, 0x00000000, 0x00390008};
// FS: MOV t1, u0  -- output the constant color from uniform u0; ps_color_out = 1.
static const std::array<uint32_t, 4> kPsCode = {0x07811009, 0x00000000, 0x00000000, 0x20390008};

// Per-vertex-color shaders. Same MOV encoding, decoded from the two above:
//   opcode MOV = 0x09 (word0[5:0]); DST_REG = word0[22:16]; DST_COMPS=0xF (xyzw);
//   MOV reads SRC2: SRC2_REG = word3[12:4], SRC2_RGROUP = word3[30:28]
//   (0 = temp, 2 = uniform -- that lone bit is the only diff between kVsCode and
//   kPsCode above). So word0 = 0x07801009 | (dst<<16), word3 = 0x00390008 |
//   (src<<4) [| 0x20000000 for a uniform].
// VS: pass position (in t0 -> out t2) and color (in t1 -> out t3, a varying).
static const std::array<uint32_t, 8> kVsColorCode = {
	// clang-format off
	0x07821009, 0x00000000, 0x00000000, 0x00390008, // MOV t2, t0  (position)
	0x07831009, 0x00000000, 0x00000000, 0x00390018, // MOV t3, t1  (color -> varying)
	// clang-format on
};
// FS: output the interpolated color varying (rasterizer lands it in t1); out t1.
static const std::array<uint32_t, 4> kPsColorCode = {0x07811009, 0x00000000, 0x00000000, 0x00390018}; // MOV t1, t1

// One-time HALTI5 pipe init (subset of etna_reset_gpu_state). Idempotent state,
// so we just emit it ahead of the draw.
static void emit_reset(CmdStream &cs)
{
	cs.set_state(GL_API_MODE, 0); // OPENGL
	cs.set_state(PA_W_CLIP_LIMIT, 0x34000001);
	cs.set_state(PA_FLAGS, 0);
	cs.set_state(PA_VIEWPORT_UNK00A80, 0x38A01404);
	cs.set_state(PA_VIEWPORT_UNK00A84, fui(8192.0f));
	cs.set_state(PA_ZFARCLIPPING, 0);
	cs.set_state(RA_HDEPTH_CONTROL, 0x7000);
	cs.set_state(PS_CONTROL_EXT, 0);
	cs.set_state(VS_HALTI1_UNK00884, 0x808);
	cs.set_state(PS_HALTI3_UNK0103C, 0x76543210);
	cs.set_state(PE_HALTI4_UNK014C0, 0);
	cs.set_state(NTE_DESCRIPTOR_CONTROL, 1); // ENABLE
	cs.set_state(FE_HALTI5_UNK007D8, 2);
	cs.set_state(PS_SAMPLER_BASE, 0);
	cs.set_state(VS_SAMPLER_BASE, 0x20);
	cs.set_state(SH_CONFIG, SH_CONFIG_RTNE);
	cs.set_state(RS_SINGLE_BUFFER, 1);
	cs.set_state(VS_ICACHE_INVALIDATE, 0x1F); // UNK0..UNK4
}

// Emit the full per-draw state + shader upload + DRAW for a position-only VS +
// constant-color FS, one render target, depth/blend/cull off, no varyings.
static void emit_triangle(CmdStream &cs,
						  const Bo &rt,
						  uint32_t rt_stride,
						  const Bo &vtx,
						  uint32_t vtx_stride,
						  const Bo &vs,
						  const Bo &ps,
						  uint32_t width,
						  uint32_t height,
						  const float color[4],
						  uint32_t vertex_count)
{
	emit_reset(cs);

	// --- flush + stall before changing framebuffer/pipe state ----------------
	cs.flush_cache();
	cs.stall(SYNC_RECIPIENT_RA, SYNC_RECIPIENT_PE);

	// --- vertex input (NFE): one attribute = position, R32G32B32_FLOAT --------
	// CONFIG0: TYPE_FLOAT(8) | NUM(3)<<12 | STREAM(0)<<8 | START(0)<<16
	cs.set_state(NFE_ATTRIB_CONFIG0_0, NFE_TYPE_FLOAT | (3u << 12));
	cs.set_state(NFE_ATTRIB_SCALE0, fui(1.0f));
	// CONFIG1: NONCONSECUTIVE(0x800) | END(bytespan = 12)
	cs.set_state(NFE_ATTRIB_CONFIG1_0, 0x800u | 12u);
	cs.set_state_reloc(NFE_VERTEX_STREAM_BASE0, {&vtx, RelocRead, 0});
	cs.set_state(NFE_VERTEX_STREAM_CONTROL0, vtx_stride); // stride (12)
	cs.set_state(NFE_VERTEX_STREAM_DIVISOR0, 0);

	// --- MSAA off ------------------------------------------------------------
	cs.set_state(GL_MULTI_SAMPLE_CONFIG, 0); // MSAA_SAMPLES_NONE

	// --- VS config -----------------------------------------------------------
	cs.set_state(VS_OUTPUT_COUNT, 1);		   // 1 + num_varyings
	cs.set_state(VS_INPUT_COUNT, 0x101);	   // COUNT(1) | UNK8(1)
	cs.set_state(VS_TEMP_REGISTER_CONTROL, 2); // NUM_TEMPS
	cs.set_state(VS_LOAD_BALANCING, 0x0F3F0241);

	// --- PA: viewport (full target), config ----------------------------------
	set_state_fixp(cs, PA_VIEWPORT_SCALE_X, fixp16(width / 2.0f));
	set_state_fixp(cs, PA_VIEWPORT_SCALE_Y, fixp16(height / 2.0f));
	cs.set_state(PA_VIEWPORT_SCALE_Z, fui(1.0f)); // scaleZ*2 (float, not fixp)
	set_state_fixp(cs, PA_VIEWPORT_OFFSET_X, fixp16(width / 2.0f));
	set_state_fixp(cs, PA_VIEWPORT_OFFSET_Y, fixp16(height / 2.0f));
	cs.set_state(PA_VIEWPORT_OFFSET_Z, fui(0.0f)); // translateZ - scaleZ (float)
	cs.set_state(PA_LINE_WIDTH, fui(0.5f));
	cs.set_state(PA_POINT_SIZE, fui(0.5f));
	cs.set_state(PA_SYSTEM_MODE, 0x1);
	cs.set_state(PA_ATTRIBUTE_ELEMENT_COUNT, 0); // num varyings
	cs.set_state(PA_CONFIG, PA_CONFIG_TRIANGLE); // SMOOTH | CULL_OFF | FILL_SOLID
	cs.set_state(PA_WIDE_LINE_WIDTH0, fui(0.5f));
	cs.set_state(PA_WIDE_LINE_WIDTH1, fui(0.5f));

	// --- SE: scissor + clip = full target ------------------------------------
	set_state_fixp(cs, SE_SCISSOR_LEFT, 0);
	set_state_fixp(cs, SE_SCISSOR_TOP, 0);
	set_state_fixp(cs, SE_SCISSOR_RIGHT, (width << 16) + SE_SCISSOR_MARGIN_RIGHT);
	set_state_fixp(cs, SE_SCISSOR_BOTTOM, (height << 16) + SE_SCISSOR_MARGIN_BOTTOM);
	cs.set_state(SE_DEPTH_SCALE, 0);
	cs.set_state(SE_DEPTH_BIAS, 0);
	cs.set_state(SE_CONFIG, 0);
	set_state_fixp(cs, SE_CLIP_RIGHT, (width << 16) + SE_CLIP_MARGIN_RIGHT);
	set_state_fixp(cs, SE_CLIP_BOTTOM, (height << 16) + SE_CLIP_MARGIN_BOTTOM);

	// --- RA: rasterizer, depth off -------------------------------------------
	cs.set_state(RA_CONTROL, 0x1);
	cs.set_state(RA_EARLY_DEPTH, RA_EARLY_DEPTH_DISABLED);

	// --- PS config -----------------------------------------------------------
	cs.set_state(PS_OUTPUT_REG, 1);			   // ps_color_out_reg
	cs.set_state(PS_INPUT_COUNT, 0x101);	   // COUNT(1) | UNK8(1)
	cs.set_state(PS_TEMP_REGISTER_CONTROL, 2); // NUM_TEMPS
	cs.set_state(PS_CONTROL, 0x2);			   // SATURATE_RT0 (unorm)

	// --- PE: render target, depth disabled -----------------------------------
	cs.set_state(PE_DEPTH_CONFIG, PE_DEPTH_CONFIG_DISABLED);
	cs.set_state(PE_DEPTH_NEAR, fui(0.0f));
	cs.set_state(PE_DEPTH_FAR, fui(1.0f));
	cs.set_state(PE_DEPTH_NORMALIZE, 0);
	cs.set_state(PE_DEPTH_STRIDE, 0);
	cs.set_state(PE_STENCIL_OP, 0);
	cs.set_state(PE_STENCIL_CONFIG, 0);
	cs.set_state(PE_ALPHA_OP, 0);
	cs.set_state(PE_ALPHA_BLEND_COLOR, 0);
	cs.set_state(PE_ALPHA_CONFIG, 0);
	// BGRA8888 tiled RT: FORMAT(6) | COMPONENTS_ALL | OVERWRITE (no super-tiled).
	cs.set_state(PE_COLOR_FORMAT, PE_FORMAT_A8R8G8B8 | PE_COLOR_FORMAT_COMPONENTS_ALL | PE_COLOR_FORMAT_OVERWRITE);
	cs.set_state(PE_COLOR_STRIDE, rt_stride);
	cs.set_state(PE_HDEPTH_CONTROL, 0);
	cs.set_state_reloc(PE_PIPE_COLOR_ADDR0, {&rt, static_cast<uint32_t>(RelocRead | RelocWrite), 0});
	cs.set_state(PE_STENCIL_CONFIG_EXT, 0);
	cs.set_state(PE_LOGIC_OP, PE_LOGIC_OP_COPY_SINGLEBUF);
	cs.set_state(PE_DITHER0, 0xFFFFFFFF);
	cs.set_state(PE_DITHER1, 0xFFFFFFFF);
	cs.set_state(PE_STENCIL_CONFIG_EXT2, 0);
	cs.set_state(PE_MEM_CONFIG, 0);

	// --- HALTI5 shader linkage (no varyings) ---------------------------------
	cs.set_state(FE_HALTI5_ID_CONFIG, 0);
	cs.set_state(VS_HALTI5_OUTPUT_COUNT, 0x1001);
	cs.set_state(VS_HALTI5_UNK008A0, 0x1101000E);
	cs.set_state(VS_HALTI5_OUTPUT0, 0x01); // vs_pos_out_reg in byte 0
	cs.set_state(VS_HALTI5_INPUT0, 0x00);  // position attr -> t0
	cs.set_state(PA_VS_OUTPUT_COUNT, 1);
	cs.set_state(PA_VARYING_NUM_COMPONENTS0, 0);
	cs.set_state(PA_VARYING_NUM_COMPONENTS1, 0);
	cs.set_state(PS_VARYING_NUM_COMPONENTS0, 0);
	cs.set_state(PS_VARYING_NUM_COMPONENTS1, 0);
	cs.set_state(GL_VARYING_TOTAL_COMPONENTS, 0);
	cs.set_state(GL_HALTI5_SH_SPECIALS, 0x7F7F7F00);

	// --- FE -> PE stall (shader state about to change) -----------------------
	cs.stall(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);

	// --- shader ICACHE upload (VS then PS) -----------------------------------
	cs.set_state(VS_NEWRANGE_LOW, 0);
	cs.set_state(VS_HALTI5_RANGE_HIGH, 1); // inst_words/4
	cs.set_state_reloc(VS_INST_ADDR, {&vs, RelocRead, 0});
	cs.set_state(SH_CONFIG, SH_CONFIG_RTNE);
	cs.set_state(SH_ICACHE_CONTROL, SH_ICACHE_CONTROL_ENABLE);
	cs.set_state(VS_ICACHE_COUNT, 0); // inst_words/4 - 1
	cs.set_state(PS_NEWRANGE_LOW, 0);
	cs.set_state(PS_HALTI5_RANGE_HIGH, 1);
	cs.set_state_reloc(PS_INST_ADDR, {&ps, RelocRead, 0});
	cs.set_state(SH_CONFIG, SH_CONFIG_RTNE);
	cs.set_state(SH_ICACHE_CONTROL, SH_ICACHE_CONTROL_ENABLE);
	cs.set_state(PS_ICACHE_COUNT, 0);

	// --- uniforms: VS none, PS color = u0 (RGBA) -----------------------------
	cs.set_state(VS_UNIFORM_BASE, 0);
	cs.set_state(PS_UNIFORM_BASE, 0); // vs_uniform_count/4
	// PS color -> u0 at the PS unified-uniform bank (0x36000).
	cs.emit(cmd_load_state(SH_HALTI5_UNIFORMS0, 4));
	cs.emit(fui(color[0]));
	cs.emit(fui(color[1]));
	cs.emit(fui(color[2]));
	cs.emit(fui(color[3]));
	cs.align();

	// --- ICACHE prefetch + RA -> PE stall ------------------------------------
	cs.set_state(VS_ICACHE_PREFETCH, 0);
	cs.set_state(PS_ICACHE_PREFETCH, 0);
	cs.stall(SYNC_RECIPIENT_RA, SYNC_RECIPIENT_PE);

	// --- DRAW_INSTANCED: triangles, 1 instance, `vertex_count` verts ---------
	cs.emit(FE_DRAW_INSTANCED | (PRIM_TRIANGLES << 16) | 1); // instanceCount lo = 1
	cs.emit(vertex_count & 0x00FFFFFF);
	cs.emit(0); // start index
	cs.emit(0); // pad

	// --- drain: PE done + flushed to DDR (submit adds the ring event) --------
	cs.stall(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
	cs.flush_cache();
	cs.stall(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
}

// v1 test: clear a small tiled RT to blue, draw a screen-covering red triangle,
// read the RT back directly (solid color -> tiling-invariant) and report how
// many pixels changed from clear -> triangle color.
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

	const float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};

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
	return true;
}

// =============================================================================
//  Per-vertex color: a real varying interpolated across the triangle
// =============================================================================
//
// Two vertex attributes now (position + color), interleaved in one stream. The
// VS passes the color through as a smooth vec4 varying; the rasterizer
// interpolates it per fragment; the FS just outputs it. Result is an RGB
// gradient over the three differently-colored vertices.
//
// The HALTI5 varying-linkage magic values are computed the way Mesa's
// etna_link_shaders() / emit_halti5_only_state() do, for num_varyings=1,
// 4 components, smooth interpolation, vs_output_count = 1 + 1 = 2:
//   VS_HALTI5_OUTPUT_COUNT = n | ((n*0x10)<<8)          = 0x2002
//   VS_HALTI5_UNK008A0     = 0x0001000e | ((0x110/n)<<20) = 0x0881000e
static void emit_triangle_color(CmdStream &cs,
								const Bo &rt,
								uint32_t rt_stride,
								const Bo &vtx,
								uint32_t vtx_stride,
								const Bo &vs,
								const Bo &ps,
								uint32_t width,
								uint32_t height,
								uint32_t vertex_count)
{
	emit_reset(cs);

	cs.flush_cache();
	cs.stall(SYNC_RECIPIENT_RA, SYNC_RECIPIENT_PE);

	// --- vertex input (NFE): 2 attributes interleaved in stream 0 ------------
	// The two attributes are byte-consecutive (pos ends at 12 = color start), so
	// Mesa groups them: attr0 is CONSECUTIVE (no 0x800), attr1 closes the group
	// (NONCONSECUTIVE) and its END is the full 28-byte span from the group start.
	// attr0 = position, R32G32B32_FLOAT at byte 0:
	cs.set_state(NFE_ATTRIB_CONFIG0_0 + 0, NFE_TYPE_FLOAT | (3u << 12)); // 0x00003008
	cs.set_state(NFE_ATTRIB_SCALE0 + 0, fui(1.0f));
	cs.set_state(NFE_ATTRIB_CONFIG1_0 + 0, 12u); // END=12
	// attr1 = color, R32G32B32A32_FLOAT at byte 12:
	cs.set_state(NFE_ATTRIB_CONFIG0_0 + 4, NFE_TYPE_FLOAT | (4u << 12) | (12u << 16)); // 0x000C4008
	cs.set_state(NFE_ATTRIB_SCALE0 + 4, fui(1.0f));
	cs.set_state(NFE_ATTRIB_CONFIG1_0 + 4, 0x800u | 28u); // NONCONSEC | END=28
	cs.set_state_reloc(NFE_VERTEX_STREAM_BASE0, {&vtx, RelocRead, 0});
	cs.set_state(NFE_VERTEX_STREAM_CONTROL0, vtx_stride); // stride = 28
	cs.set_state(NFE_VERTEX_STREAM_DIVISOR0, 0);

	cs.set_state(GL_MULTI_SAMPLE_CONFIG, 0);

	// --- VS config: 2 inputs, 2 outputs (position + 1 varying) ---------------
	cs.set_state(VS_OUTPUT_COUNT, 2);		   // 1 + num_varyings
	cs.set_state(VS_INPUT_COUNT, 0x102);	   // COUNT(2) | UNK8(1)
	cs.set_state(VS_TEMP_REGISTER_CONTROL, 4); // t0,t1 in; t2,t3 out
	cs.set_state(VS_LOAD_BALANCING, 0x0F3F0241);

	// --- PA viewport (same as the solid path) --------------------------------
	set_state_fixp(cs, PA_VIEWPORT_SCALE_X, fixp16(width / 2.0f));
	set_state_fixp(cs, PA_VIEWPORT_SCALE_Y, fixp16(height / 2.0f));
	cs.set_state(PA_VIEWPORT_SCALE_Z, fui(1.0f));
	set_state_fixp(cs, PA_VIEWPORT_OFFSET_X, fixp16(width / 2.0f));
	set_state_fixp(cs, PA_VIEWPORT_OFFSET_Y, fixp16(height / 2.0f));
	cs.set_state(PA_VIEWPORT_OFFSET_Z, fui(0.0f));
	cs.set_state(PA_LINE_WIDTH, fui(0.5f));
	cs.set_state(PA_POINT_SIZE, fui(0.5f));
	cs.set_state(PA_SYSTEM_MODE, 0x1);
	cs.set_state(PA_ATTRIBUTE_ELEMENT_COUNT, 1); // num varyings = 1
	cs.set_state(PA_CONFIG, PA_CONFIG_TRIANGLE);
	cs.set_state(PA_WIDE_LINE_WIDTH0, fui(0.5f));
	cs.set_state(PA_WIDE_LINE_WIDTH1, fui(0.5f));

	// --- SE scissor + clip (same) --------------------------------------------
	set_state_fixp(cs, SE_SCISSOR_LEFT, 0);
	set_state_fixp(cs, SE_SCISSOR_TOP, 0);
	set_state_fixp(cs, SE_SCISSOR_RIGHT, (width << 16) + SE_SCISSOR_MARGIN_RIGHT);
	set_state_fixp(cs, SE_SCISSOR_BOTTOM, (height << 16) + SE_SCISSOR_MARGIN_BOTTOM);
	cs.set_state(SE_DEPTH_SCALE, 0);
	cs.set_state(SE_DEPTH_BIAS, 0);
	cs.set_state(SE_CONFIG, 0);
	set_state_fixp(cs, SE_CLIP_RIGHT, (width << 16) + SE_CLIP_MARGIN_RIGHT);
	set_state_fixp(cs, SE_CLIP_BOTTOM, (height << 16) + SE_CLIP_MARGIN_BOTTOM);

	// --- RA (same) -----------------------------------------------------------
	cs.set_state(RA_CONTROL, 0x1);
	cs.set_state(RA_EARLY_DEPTH, RA_EARLY_DEPTH_DISABLED);

	// --- PS config: 1 varying input, color out in t1 -------------------------
	cs.set_state(PS_OUTPUT_REG, 1);		 // ps_color_out_reg
	cs.set_state(PS_INPUT_COUNT, 0x102); // COUNT(num_varyings+1=2) | UNK8(1)
	cs.set_state(PS_TEMP_REGISTER_CONTROL, 2);
	cs.set_state(PS_CONTROL, 0x2); // SATURATE_RT0 (unorm)

	// --- PE render target (same) ---------------------------------------------
	cs.set_state(PE_DEPTH_CONFIG, PE_DEPTH_CONFIG_DISABLED);
	cs.set_state(PE_DEPTH_NEAR, fui(0.0f));
	cs.set_state(PE_DEPTH_FAR, fui(1.0f));
	cs.set_state(PE_DEPTH_NORMALIZE, 0);
	cs.set_state(PE_DEPTH_STRIDE, 0);
	cs.set_state(PE_STENCIL_OP, 0);
	cs.set_state(PE_STENCIL_CONFIG, 0);
	cs.set_state(PE_ALPHA_OP, 0);
	cs.set_state(PE_ALPHA_BLEND_COLOR, 0);
	cs.set_state(PE_ALPHA_CONFIG, 0);
	cs.set_state(PE_COLOR_FORMAT, PE_FORMAT_A8R8G8B8 | PE_COLOR_FORMAT_COMPONENTS_ALL | PE_COLOR_FORMAT_OVERWRITE);
	cs.set_state(PE_COLOR_STRIDE, rt_stride);
	cs.set_state(PE_HDEPTH_CONTROL, 0);
	cs.set_state_reloc(PE_PIPE_COLOR_ADDR0, {&rt, static_cast<uint32_t>(RelocRead | RelocWrite), 0});
	cs.set_state(PE_STENCIL_CONFIG_EXT, 0);
	cs.set_state(PE_LOGIC_OP, PE_LOGIC_OP_COPY_SINGLEBUF);
	cs.set_state(PE_DITHER0, 0xFFFFFFFF);
	cs.set_state(PE_DITHER1, 0xFFFFFFFF);
	cs.set_state(PE_STENCIL_CONFIG_EXT2, 0);
	cs.set_state(PE_MEM_CONFIG, 0);

	// --- HALTI5 shader linkage: 1 smooth vec4 varying ------------------------
	cs.set_state(FE_HALTI5_ID_CONFIG, 0);
	cs.set_state(VS_HALTI5_OUTPUT_COUNT, 0x2002); // vs_output_count = 2
	cs.set_state(VS_HALTI5_UNK008A0, 0x0881000E); // 0x0001000e | ((0x110/2)<<20)
	cs.set_state(VS_HALTI5_OUTPUT0, 0x0302);	  // pos=t2 (byte0), varying=t3 (byte1)
	cs.set_state(VS_HALTI5_INPUT0, 0x0100);		  // attr0->t0 (byte0), attr1->t1 (byte1)
	cs.set_state(PA_VS_OUTPUT_COUNT, 2);
	cs.set_state(PA_VARYING_NUM_COMPONENTS0, 4); // varying0 has 4 components
	cs.set_state(PA_VARYING_NUM_COMPONENTS1, 0);
	cs.set_state(PS_VARYING_NUM_COMPONENTS0, 4);
	cs.set_state(PS_VARYING_NUM_COMPONENTS1, 0);
	cs.set_state(GL_VARYING_TOTAL_COMPONENTS, 4); // align(total_components=4, 2)
	cs.set_state(GL_HALTI5_SH_SPECIALS, 0x7F7F7F00);
	cs.set_state(GL_HALTI5_SHADER_ATTRIBUTES0, 0); // 4 comps x SMOOTH(0) semantic

	cs.stall(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);

	// --- shader ICACHE upload (VS = 2 insts = 8 words, PS = 1 inst) ----------
	cs.set_state(VS_NEWRANGE_LOW, 0);
	cs.set_state(VS_HALTI5_RANGE_HIGH, 2); // inst_words(8) / 4
	cs.set_state_reloc(VS_INST_ADDR, {&vs, RelocRead, 0});
	cs.set_state(SH_CONFIG, SH_CONFIG_RTNE);
	cs.set_state(SH_ICACHE_CONTROL, SH_ICACHE_CONTROL_ENABLE);
	cs.set_state(VS_ICACHE_COUNT, 1); // inst_words/4 - 1
	cs.set_state(PS_NEWRANGE_LOW, 0);
	cs.set_state(PS_HALTI5_RANGE_HIGH, 1);
	cs.set_state_reloc(PS_INST_ADDR, {&ps, RelocRead, 0});
	cs.set_state(SH_CONFIG, SH_CONFIG_RTNE);
	cs.set_state(SH_ICACHE_CONTROL, SH_ICACHE_CONTROL_ENABLE);
	cs.set_state(PS_ICACHE_COUNT, 0);

	// --- no uniforms: the color rides the varying ----------------------------
	cs.set_state(VS_UNIFORM_BASE, 0);
	cs.set_state(PS_UNIFORM_BASE, 0);

	cs.set_state(VS_ICACHE_PREFETCH, 0);
	cs.set_state(PS_ICACHE_PREFETCH, 0);
	cs.stall(SYNC_RECIPIENT_RA, SYNC_RECIPIENT_PE);

	// --- DRAW ----------------------------------------------------------------
	cs.emit(FE_DRAW_INSTANCED | (PRIM_TRIANGLES << 16) | 1);
	cs.emit(vertex_count & 0x00FFFFFF);
	cs.emit(0);
	cs.emit(0);

	cs.stall(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
	cs.flush_cache();
	cs.stall(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
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

} // namespace etna
