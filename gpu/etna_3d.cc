#include "aarch64/system_reg.hh" // read_cntpct
#include "etna.hh"
#include "gpu_regs.hh"
#include "gpu_regs_3d.hh"
#include "print/print.hh"

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
static const uint32_t kVsCode[4] = {0x07811009, 0x00000000, 0x00000000, 0x00390008};
// FS: MOV t1, u0  -- output the constant color from uniform u0; ps_color_out = 1.
static const uint32_t kPsCode[4] = {0x07811009, 0x00000000, 0x00000000, 0x20390008};

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
	cs.set_state(VS_OUTPUT_COUNT, 1);          // 1 + num_varyings
	cs.set_state(VS_INPUT_COUNT, 0x101);       // COUNT(1) | UNK8(1)
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
	cs.set_state(PS_OUTPUT_REG, 1);            // ps_color_out_reg
	cs.set_state(PS_INPUT_COUNT, 0x101);       // COUNT(1) | UNK8(1)
	cs.set_state(PS_TEMP_REGISTER_CONTROL, 2); // NUM_TEMPS
	cs.set_state(PS_CONTROL, 0x2);             // SATURATE_RT0 (unorm)

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
	const uint32_t pw = (W + 15) & ~15u; // pad width to 16
	const uint32_t ph = (H + 3) & ~3u;   // pad height to 4
	const uint32_t stride = pw * 4;
	const uint32_t rt_size = stride * ph;
	constexpr uint32_t CLEAR = 0xFF0000FF; // blue (B,G,R,A = FF,00,00,FF)

	Bo rt = gpu.alloc(rt_size);
	Bo vtx = gpu.alloc(3 * 3 * 4); // 3 verts x vec3 float
	Bo vs = gpu.alloc(sizeof(kVsCode));
	Bo ps = gpu.alloc(sizeof(kPsCode));
	if (!rt || !vtx || !vs || !ps)
		return false;

	// clear RT (solid -> same in every byte regardless of tiling)
	auto rp = static_cast<uint32_t *>(rt.map());
	for (uint32_t i = 0; i < rt_size / 4; i++)
		rp[i] = CLEAR;
	rt.cpu_fini(RelocWrite);

	// a triangle fully INSIDE the clip volume (no fullscreen/guardband trick),
	// z = 0.5 (mid depth range, off the near plane). Covers most of the screen.
	const float verts[9] = {-0.8f, -0.8f, 0.5f, 0.8f, -0.8f, 0.5f, 0.0f, 0.8f, 0.5f};
	auto vp = static_cast<float *>(vtx.map());
	for (int i = 0; i < 9; i++)
		vp[i] = verts[i];
	vtx.cpu_fini(RelocWrite);

	auto vsp = static_cast<uint32_t *>(vs.map());
	for (int i = 0; i < 4; i++)
		vsp[i] = kVsCode[i];
	vs.cpu_fini(RelocWrite);
	auto psp = static_cast<uint32_t *>(ps.map());
	for (int i = 0; i < 4; i++)
		psp[i] = kPsCode[i];
	ps.cpu_fini(RelocWrite);

	const float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};

	auto cs = gpu.new_cmd_stream(1024);
	emit_triangle(cs, rt, stride, vtx, 12, vs, ps, W, H, red, 3);

	auto start = read_cntpct();
	if (!gpu.submit_and_wait(cs)) {
		gpu.dump_status("triangle draw");
		return false;
	}
	print("3D triangle drawn in ", (uint32_t)(read_cntpct() - start), " ticks\n");
	gpu.dump_status("after 3D draw"); // DEBUG: PE/FE/IAC/RISAF state -- did the PE fault or just draw nothing?

	rt.cpu_prep(RelocRead);
	uint32_t drawn = 0, sample = 0;
	for (uint32_t i = 0; i < rt_size / 4; i++)
		if (rp[i] != CLEAR) {
			if (!drawn)
				sample = rp[i];
			drawn++;
		}
	print("RT pixels changed from clear: ", int(drawn), " of ", int(rt_size / 4));
	print("  (first drawn pixel = 0x", Hex{sample}, ", clear was 0x", Hex{CLEAR}, ")\n");
	if (drawn == 0) {
		print("FAILED: the draw wrote no pixels (pipe/state issue)\n");
		return false;
	}
	print("GPU 3D pipe wrote pixels -- triangle path is alive. \\o/\n");
	return true;
}

} // namespace etna
