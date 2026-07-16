#pragma once
#include <cstdint>

// =============================================================================
//  gpu_regs_3d.hh -- Vivante 3D graphics-pipe registers (HALTI5 / GCNanoUltra31)
// =============================================================================
//
// Net-new register map for the programmable 3D pipeline (vertex fetch ->
// vertex shader -> rasterizer -> fragment shader -> pixel engine), kept
// separate from the RS/MMU/compute registers in gpu_regs.hh.
//
// Addresses are the etnaviv VIVS_* register names from Mesa's
// src/etnaviv/hw/state_3d.xml.h (MIT). Values/sequences are the minimal
// one-triangle path extracted from the Mesa etnaviv driver's etna_emit_state()
// / etna_draw_vbo() for OUR exact chip (customer 0x15, HALTI5, 2 shader cores,
// 1 pixel pipe, no BLT -> RS resolve, LinearPE but PE still renders TILED).
//
// Register domains: PA (primitive assembly/viewport), SE (setup/scissor),
// RA (rasterizer), PE (pixel engine / render target), VS/PS (shader config),
// SH (shader instruction cache + uniforms), GL (global/sync), FE+NFE (vertex
// input + draw). All are written via CmdStream::set_state(addr, value).

namespace VivanteGpu
{

// ---- FE draw command (cmdstream.xml.h) --------------------------------------
// HALTI2+ always uses DRAW_INSTANCED (opcode 0xC, header 0x60000000). Its count
// field is a VERTEX count, not a primitive count. word0 = header | (indexed?
// 0x00100000:0) | (primType<<16) | (instanceCount & 0xffff); word1 =
// ((instanceCount>>16)<<24) | (vertexCount & 0xffffff); word2 = startIndex;
// word3 = 0 (pad).
constexpr uint32_t FE_DRAW_INSTANCED = 0x60000000;
constexpr uint32_t PRIM_TRIANGLES = 4; // PRIMITIVE_TYPE_TRIANGLES
constexpr uint32_t PRIM_TRIANGLE_STRIP = 5;

// Sync recipient for the RA (rasterizer) stage; FE/PE are in gpu_regs.hh
// (SYNC_RECIPIENT_FE/PE). Used with GL_SEMAPHORE_TOKEN / the STALL command.
constexpr uint32_t SYNC_RECIPIENT_RA = 5;

// ---- GL / global (GL_SEMAPHORE_TOKEN / GL_FLUSH_CACHE are in gpu_regs.hh) ----
constexpr uint32_t GL_MULTI_SAMPLE_CONFIG = 0x3818;
constexpr uint32_t GL_VARYING_TOTAL_COMPONENTS = 0x381C;
constexpr uint32_t GL_API_MODE = 0x384C; // OPENGL = 0
constexpr uint32_t GL_HALTI5_SH_SPECIALS = 0x3888;
constexpr uint32_t GL_HALTI5_SHADER_ATTRIBUTES0 = 0x38C0;
constexpr uint32_t GL_STALL_TOKEN = 0x3C00;

// ---- FE / NFE vertex input (HALTI5 uses NFE, not the legacy FE_VERTEX_*) -----
constexpr uint32_t FE_HALTI5_ID_CONFIG = 0x07C4;  // vertex/instance-id feed (0 = none)
constexpr uint32_t FE_HALTI5_UNK007D8 = 0x07D8;   // reset-time = 2
constexpr uint32_t NFE_ATTRIB_CONFIG0_0 = 0x17800; // TYPE|NUM|STREAM|START
constexpr uint32_t NFE_ATTRIB_SCALE0 = 0x17A00;    // fui(1.0) for float attr
constexpr uint32_t NFE_ATTRIB_CONFIG1_0 = 0x17A80; // NONCONSECUTIVE|END(bytespan)
constexpr uint32_t NFE_VERTEX_STREAM_BASE0 = 0x14600; // reloc: vertex buffer base
constexpr uint32_t NFE_VERTEX_STREAM_CONTROL0 = 0x14640; // stride
constexpr uint32_t NFE_VERTEX_STREAM_DIVISOR0 = 0x14680;
// NFE_ATTRIB_CONFIG0 fields: TYPE_FLOAT=8 in [3:0], NUM<<12, ENDIAN<<4,
// STREAM<<8, START<<16.  CONFIG1: NONCONSECUTIVE=0x800, END(bytespan) in [15:0].
constexpr uint32_t NFE_TYPE_FLOAT = 8;

// ---- VS (vertex shader config) ----------------------------------------------
constexpr uint32_t VS_OUTPUT_COUNT = 0x0804;         // 1 + num_varyings
constexpr uint32_t VS_INPUT_COUNT = 0x0808;          // COUNT | UNK8<<8
constexpr uint32_t VS_TEMP_REGISTER_CONTROL = 0x080C; // NUM_TEMPS
constexpr uint32_t VS_LOAD_BALANCING = 0x0830;
constexpr uint32_t VS_UNIFORM_BASE = 0x0864;
constexpr uint32_t SH_ICACHE_CONTROL = 0x0868; // ENABLE = 1 (shared VS/PS)
constexpr uint32_t VS_INST_ADDR = 0x086C;      // reloc: VS code BO
constexpr uint32_t VS_HALTI5_OUTPUT_COUNT = 0x0870;
constexpr uint32_t VS_NEWRANGE_LOW = 0x0874;
constexpr uint32_t PS_NEWRANGE_LOW = 0x087C; // (PS reg living in the 0x08xx block)
constexpr uint32_t VS_HALTI1_UNK00884 = 0x0884;
constexpr uint32_t VS_ICACHE_PREFETCH = 0x088C;
constexpr uint32_t VS_HALTI5_UNK008A0 = 0x08A0;
constexpr uint32_t VS_SAMPLER_BASE = 0x08A8; // reset = 0x20
constexpr uint32_t VS_ICACHE_INVALIDATE = 0x08B0;
constexpr uint32_t VS_HALTI5_RANGE_HIGH = 0x08BC; // inst_words/4
constexpr uint32_t VS_HALTI5_INPUT0 = 0x08C0;
constexpr uint32_t VS_HALTI5_OUTPUT0 = 0x08E0; // byte0 = vs_pos_out_reg

// ---- PA (primitive assembly / viewport) -------------------------------------
constexpr uint32_t PA_VIEWPORT_SCALE_X = 0x0A00; // fixp16
constexpr uint32_t PA_VIEWPORT_SCALE_Y = 0x0A04; // fixp16
constexpr uint32_t PA_VIEWPORT_SCALE_Z = 0x0A08; // fui(scaleZ*2)
constexpr uint32_t PA_VIEWPORT_OFFSET_X = 0x0A0C; // fixp16
constexpr uint32_t PA_VIEWPORT_OFFSET_Y = 0x0A10; // fixp16
constexpr uint32_t PA_VIEWPORT_OFFSET_Z = 0x0A14; // fui(translateZ - scaleZ)
constexpr uint32_t PA_LINE_WIDTH = 0x0A18;
constexpr uint32_t PA_POINT_SIZE = 0x0A1C;
constexpr uint32_t PA_SYSTEM_MODE = 0x0A28;
constexpr uint32_t PA_W_CLIP_LIMIT = 0x0A2C; // reset = 0x34000001
constexpr uint32_t PA_ATTRIBUTE_ELEMENT_COUNT = 0x0A30; // num varyings
constexpr uint32_t PA_CONFIG = 0x0A34; // SHADE_SMOOTH|CULL_OFF|FILL_SOLID
constexpr uint32_t PA_WIDE_LINE_WIDTH0 = 0x0A38;
constexpr uint32_t PA_WIDE_LINE_WIDTH1 = 0x0A3C;
constexpr uint32_t PA_VIEWPORT_UNK00A80 = 0x0A80; // reset = 0x38a01404
constexpr uint32_t PA_VIEWPORT_UNK00A84 = 0x0A84; // reset = fui(8192)
constexpr uint32_t PA_FLAGS = 0x0A88;
constexpr uint32_t PA_ZFARCLIPPING = 0x0A8C;
constexpr uint32_t PA_VARYING_NUM_COMPONENTS0 = 0x0A90;
constexpr uint32_t PA_VARYING_NUM_COMPONENTS1 = 0x0A94;
constexpr uint32_t PA_VS_OUTPUT_COUNT = 0x0AA8;
// PA_CONFIG bits: SHADE_MODEL_SMOOTH=0x10000, CULL_FACE_MODE_OFF=0,
// FILL_MODE_SOLID=0x2000.
constexpr uint32_t PA_CONFIG_TRIANGLE = 0x10000 | 0x2000;

// ---- SE (setup engine / scissor / clip) -------------------------------------
constexpr uint32_t SE_SCISSOR_LEFT = 0x0C00;   // fixp16
constexpr uint32_t SE_SCISSOR_TOP = 0x0C04;    // fixp16
constexpr uint32_t SE_SCISSOR_RIGHT = 0x0C08;  // (maxx<<16)+0x1119
constexpr uint32_t SE_SCISSOR_BOTTOM = 0x0C0C; // (maxy<<16)+0x1111
constexpr uint32_t SE_DEPTH_SCALE = 0x0C10;
constexpr uint32_t SE_DEPTH_BIAS = 0x0C14;
constexpr uint32_t SE_CONFIG = 0x0C18;
constexpr uint32_t SE_CLIP_RIGHT = 0x0C20;  // (maxx<<16)+0xffff
constexpr uint32_t SE_CLIP_BOTTOM = 0x0C24; // (maxy<<16)+0xffff
constexpr uint32_t SE_SCISSOR_MARGIN_RIGHT = 0x1119;
constexpr uint32_t SE_SCISSOR_MARGIN_BOTTOM = 0x1111;
constexpr uint32_t SE_CLIP_MARGIN_RIGHT = 0xFFFF;
constexpr uint32_t SE_CLIP_MARGIN_BOTTOM = 0xFFFF;

// ---- RA (rasterizer) --------------------------------------------------------
constexpr uint32_t RA_CONTROL = 0x0E00;       // UNK0 = 1
constexpr uint32_t RA_EARLY_DEPTH = 0x0E08;   // depth-off: 0x15000030 (RAWriteDepth)
constexpr uint32_t RA_HDEPTH_CONTROL = 0x0E20; // reset = 0x7000

// ---- PS (fragment shader config) --------------------------------------------
constexpr uint32_t PS_OUTPUT_REG = 0x1004; // ps_color_out_reg
constexpr uint32_t PS_INPUT_COUNT = 0x1008; // COUNT | UNK8<<8
constexpr uint32_t PS_TEMP_REGISTER_CONTROL = 0x100C;
constexpr uint32_t PS_CONTROL = 0x1010;     // SATURATE_RT0 = 0x2 for unorm
constexpr uint32_t PS_UNIFORM_BASE = 0x1024;
constexpr uint32_t PS_INST_ADDR = 0x1028; // reloc: PS code BO
constexpr uint32_t PS_CONTROL_EXT = 0x1030;
constexpr uint32_t PS_HALTI3_UNK0103C = 0x103C; // reset = 0x76543210
constexpr uint32_t PS_ICACHE_PREFETCH = 0x1048;
constexpr uint32_t PS_SAMPLER_BASE = 0x1058; // reset = 0
constexpr uint32_t PS_VARYING_NUM_COMPONENTS0 = 0x1080;
constexpr uint32_t PS_VARYING_NUM_COMPONENTS1 = 0x1084;
constexpr uint32_t PS_HALTI5_RANGE_HIGH = 0x1090; // inst_words/4
constexpr uint32_t PS_ICACHE_COUNT = 0x1094;      // inst_words/4 - 1

// ---- PE (pixel engine / render target) --------------------------------------
constexpr uint32_t PE_DEPTH_CONFIG = 0x1400; // depth off = 0x01000700
constexpr uint32_t PE_DEPTH_NEAR = 0x1404;
constexpr uint32_t PE_DEPTH_FAR = 0x1408;   // fui(1.0)
constexpr uint32_t PE_DEPTH_NORMALIZE = 0x140C;
constexpr uint32_t PE_DEPTH_STRIDE = 0x1414;
constexpr uint32_t PE_STENCIL_OP = 0x1418;
constexpr uint32_t PE_STENCIL_CONFIG = 0x141C;
constexpr uint32_t PE_ALPHA_OP = 0x1420;
constexpr uint32_t PE_ALPHA_BLEND_COLOR = 0x1424;
constexpr uint32_t PE_ALPHA_CONFIG = 0x1428;
constexpr uint32_t PE_COLOR_FORMAT = 0x142C; // FORMAT|COMPONENTS|OVERWRITE|tiling
constexpr uint32_t PE_COLOR_STRIDE = 0x1434;
constexpr uint32_t PE_HDEPTH_CONTROL = 0x1454;
constexpr uint32_t PE_PIPE_COLOR_ADDR0 = 0x1460; // reloc: render target base
constexpr uint32_t PE_STENCIL_CONFIG_EXT = 0x14A0;
constexpr uint32_t PE_LOGIC_OP = 0x14A4; // COPY | SINGLE_BUFFER(2) = 0x000E4200
constexpr uint32_t PE_DITHER0 = 0x14A8;  // 0xffffffff
constexpr uint32_t PE_DITHER1 = 0x14AC;  // 0xffffffff
constexpr uint32_t PE_STENCIL_CONFIG_EXT2 = 0x14B8;
constexpr uint32_t PE_MEM_CONFIG = 0x14BC;
constexpr uint32_t PE_HALTI4_UNK014C0 = 0x14C0;
// PE_COLOR_FORMAT: FORMAT in [5:0], COMPONENTS mask 0xF00 (0xF=all), OVERWRITE
// 0x10000, SUPER_TILED 0x100000, SUPER_TILED_NEW 0x2000.
constexpr uint32_t PE_FORMAT_A8R8G8B8 = 6; // native BGRA8888 memory order
constexpr uint32_t PE_COLOR_FORMAT_COMPONENTS_ALL = 0xF00;
constexpr uint32_t PE_COLOR_FORMAT_OVERWRITE = 0x10000;
constexpr uint32_t PE_LOGIC_OP_COPY_SINGLEBUF = 0x000E4200;
constexpr uint32_t PE_DEPTH_CONFIG_DISABLED = 0x01000700; // NONE|ALWAYS|DISABLE_ZS
constexpr uint32_t RA_EARLY_DEPTH_DISABLED = 0x15000030;   // FORWARD_Z|W|WRITE_DISABLE

// ---- SH (shader instruction cache count + unified uniforms, high block) -----
constexpr uint32_t SH_CONFIG = 0x15600;      // RTNE_ROUNDING = 0x2
constexpr uint32_t VS_ICACHE_COUNT = 0x15604; // inst_words/4 - 1
constexpr uint32_t SH_ICACHE_CONTROL_ENABLE = 0x1;
constexpr uint32_t SH_CONFIG_RTNE = 0x2;
constexpr uint32_t SH_HALTI5_UNIFORMS_MIRROR0 = 0x34000; // VS unified uniforms
constexpr uint32_t SH_HALTI5_UNIFORMS0 = 0x36000;        // PS unified uniforms

// ---- NTE (descriptor engine, HALTI5 init) -----------------------------------
constexpr uint32_t NTE_DESCRIPTOR_CONTROL = 0x14C40; // ENABLE at init

} // namespace VivanteGpu
