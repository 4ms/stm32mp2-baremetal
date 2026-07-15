#pragma once
#include <cstdint>

// =============================================================================
//  ppu_asm.hh -- halti5 shader instruction encoder for the STM32MP25 GPU
// =============================================================================
//
// A faithful port of the gcnano vendor driver's PPU (Parallel Processing Unit =
// unified shader core) assembler -- the gckPPU_* functions in
// gc_hal_kernel_hardware_func_flop_reset.c (dual MIT/GPL; we use it under MIT).
// See gpu/reference/gcnano_flop_reset/.
//
// Each shader instruction is 4 x uint32 (128 bits). The vendor code sets fields
// with a verbose bitfield macro that reduces to setbits(word, hi, lo, value);
// this is that, transcribed. Pure integer math -- host-compilable and testable
// (see ppu_asm_test.cc) independent of any hardware.
//
// The only chip-specific dependency, SH_END_OF_BB, is 0x0 for our core
// (feature DB, GCNANOULTRA31_VIP2 / model 0x8000 rev 0x6205 customer 0x15), so
// gckPPU_IsEndOfBB is a no-op and is omitted.

namespace ppu
{

// word[hi:lo] = value  (mirrors the vendor's _SETBITS macro)
inline void setbits(uint32_t &w, unsigned hi, unsigned lo, uint32_t v)
{
	uint32_t mask = (hi - lo + 1 == 32) ? ~0u : (~(~0u << (hi - lo + 1)));
	w = (w & ~(mask << lo)) | ((v & mask) << lo);
}
inline uint32_t getbit(uint32_t v, unsigned b)
{
	return (v >> b) & 1u;
}
inline uint32_t getbits(uint32_t v, unsigned hi, unsigned lo)
{
	uint32_t mask = (hi - lo + 1 == 32) ? ~0u : (~(~0u << (hi - lo + 1)));
	return (v >> lo) & mask;
}

// Vivante "VX" write-enable and swizzle helpers (gcdVX_*)
constexpr uint32_t VX_ENABLE = 0xF;                       // all 4 components
constexpr uint32_t VX_SWIZZLE = 0 | (1 << 2) | (2 << 4) | (3 << 6); // .xyzw
constexpr uint32_t vx_swizzle2(uint32_t x, uint32_t y)
{
	return x | (y << 2) | (y << 4) | (y << 6);
}

constexpr uint32_t SH_TYPE_INVALID = ~0u; // "no instruction-type override"

// gckPPU_SetImmediate. NOTE: the vendor function omits the SRCx_USE bit, which
// flop-reset never caught because it uses no immediates -- without it the
// operand reads as 0 (verified: an AND with an immediate mask came out all-zero
// until USE was set). We set it here so the immediate operand is actually read.
inline void set_immediate(unsigned where, uint32_t value, uint32_t type, uint32_t *inst)
{
	switch (where) {
	case 0:
		setbits(inst[1], 11, 11, 1); // SRC0_USE
		setbits(inst[1], 20, 12, getbits(value, 8, 0));
		setbits(inst[1], 29, 22, getbits(value, 16, 9));
		setbits(inst[1], 30, 30, getbit(value, 17));
		setbits(inst[1], 31, 31, getbit(value, 18));
		setbits(inst[2], 2, 0, getbit(value, 19) | (type << 1));
		setbits(inst[2], 5, 3, 0x7);
		break;
	case 1:
		setbits(inst[2], 6, 6, 1); // SRC1_USE
		setbits(inst[2], 15, 7, getbits(value, 8, 0));
		setbits(inst[2], 24, 17, getbits(value, 16, 9));
		setbits(inst[2], 25, 25, getbit(value, 17));
		setbits(inst[2], 26, 26, getbit(value, 18));
		setbits(inst[2], 29, 27, getbit(value, 19) | (type << 1));
		setbits(inst[3], 2, 0, 0x7);
		break;
	case 2:
		setbits(inst[3], 3, 3, 1); // SRC2_USE
		setbits(inst[3], 12, 4, getbits(value, 8, 0));
		setbits(inst[3], 21, 14, getbits(value, 16, 9));
		setbits(inst[3], 22, 22, getbit(value, 17));
		setbits(inst[3], 23, 23, getbit(value, 18));
		setbits(inst[3], 27, 25, getbit(value, 19) | (type << 1));
		setbits(inst[3], 30, 28, 0x7);
		break;
	}
}

// gckPPU_SetInstructionType
inline void set_instruction_type(uint32_t type, uint32_t *inst)
{
	setbits(inst[1], 21, 21, getbit(type, 0));
	setbits(inst[2], 31, 30, getbits(type, 2, 1));
}

// gckPPU_AddOpCode (IsEndOfBB omitted: SH_END_OF_BB == 0 on our chip)
inline void add_opcode(uint32_t opcode, uint32_t extended, uint32_t type, uint32_t *inst)
{
	setbits(inst[0], 5, 0, getbits(opcode, 5, 0));
	setbits(inst[2], 16, 16, getbit(opcode, 6));

	switch (opcode) {
	case 0x7F:
		set_immediate(2, extended, 0x2, inst);
		break;
	case 0x45:
		setbits(inst[0], 15, 13, getbits(extended, 2, 0));
		setbits(inst[0], 31, 31, getbit(extended, 3));
		setbits(inst[1], 1, 0, getbits(extended, 5, 4));
		break;
	case 0x31:
	case 0x09:
	case 0x0F:
		setbits(inst[0], 10, 6, getbits(extended, 4, 0));
		break;
	default:
		break;
	}

	if (type != SH_TYPE_INVALID)
		set_instruction_type(type, inst);
}

// gckPPU_SetDestination
inline void set_destination(uint32_t address, uint32_t write_enable, uint32_t saturate, uint32_t *inst)
{
	setbits(inst[0], 12, 12, 1);
	setbits(inst[0], 22, 16, address);
	setbits(inst[0], 26, 23, write_enable);
	setbits(inst[0], 11, 11, saturate);
}

// gckPPU_SetEVIS  (note [26:23] overlaps SetDestination's write_enable -- the
// vendor calls SetDestination then SetEVIS, so Start wins; we keep that order)
inline void set_evis(uint32_t start, uint32_t end, uint32_t evis, uint32_t *inst)
{
	setbits(inst[0], 26, 23, start);
	setbits(inst[0], 30, 27, end);
	setbits(inst[1], 10, 2, evis);
}

// gckPPU_SetSource
inline void set_source(unsigned where, uint32_t address, uint32_t swizzle, uint32_t type, bool negate,
					   bool absolute, uint32_t relative, uint32_t *inst)
{
	switch (where) {
	case 0:
		setbits(inst[1], 11, 11, 1);
		setbits(inst[1], 20, 12, address);
		setbits(inst[1], 29, 22, swizzle);
		setbits(inst[1], 30, 30, negate);
		setbits(inst[1], 31, 31, absolute);
		setbits(inst[2], 2, 0, relative);
		setbits(inst[2], 5, 3, type);
		break;
	case 1:
		setbits(inst[2], 6, 6, 1);
		setbits(inst[2], 15, 7, address);
		setbits(inst[2], 24, 17, swizzle);
		setbits(inst[2], 25, 25, negate);
		setbits(inst[2], 26, 26, absolute);
		setbits(inst[2], 29, 27, relative);
		setbits(inst[3], 2, 0, type);
		break;
	case 2:
		setbits(inst[3], 3, 3, 1);
		setbits(inst[3], 12, 4, address);
		setbits(inst[3], 21, 14, swizzle);
		setbits(inst[3], 22, 22, negate);
		setbits(inst[3], 23, 23, absolute);
		setbits(inst[3], 27, 25, relative);
		setbits(inst[3], 30, 28, type);
		break;
	}
}
// gckPPU_SetUniform / SetTempReg (Modifiers unused here -> negate/abs = 0)
inline void set_uniform(unsigned where, uint32_t addr, uint32_t swz, uint32_t /*mods*/, uint32_t *inst)
{
	set_source(where, addr, swz, 0x2, false, false, 0, inst);
}
inline void set_tempreg(unsigned where, uint32_t addr, uint32_t swz, uint32_t /*mods*/, uint32_t *inst)
{
	set_source(where, addr, swz, 0x0, false, false, 0, inst);
}

// gckPPU_GetPixel
inline uint32_t get_pixel(uint32_t fmt)
{
	return (fmt == 0x3 || fmt == 0x6) ? 7 : 15;
}

// A built shader: its size in dwords and how many temp registers it uses.
// Both feed the dispatch (InstCount / RegCount). A "kernel" is one of these
// builders; add more to run other computations.
struct ShaderInfo {
	unsigned inst_dwords; // total dwords (4 per instruction)
	unsigned reg_count;	  // temp registers the shader uses
};

// Build the vendor flop-reset kernel: img_load x2 -> dp2x8 -> img_store, a
// faithful copy of _ProgramPPUInstruction. NOTE despite the vendor's comment
// this is NOT a per-element add: dp2x8 is a dot-product (out[j] = sum of
// src0[k]*src1[k] over a pair), which only looks like 2*in when every input is
// equal. Kept as-is because it's the known-good flop-toggle the vendor ships.
inline ShaderInfo build_dp2x8_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	unsigned n = 0;

	// img_load.u8 r1, c0, r0.xy
	add_opcode(0x79, 0, dataType, &inst[n]);
	set_destination(1, VX_ENABLE, 0, &inst[n]);
	set_evis(0, get_pixel(dataType), 1, &inst[n]);
	set_uniform(0, 0, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	n += 4;

	// img_load.u8 r2, c0, r0.xy
	add_opcode(0x79, 0, dataType, &inst[n]);
	set_destination(2, VX_ENABLE, 0, &inst[n]);
	set_evis(0, get_pixel(dataType), 1, &inst[n]);
	set_uniform(0, 0, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	n += 4;

	// dp2x8 r1, r1, r2, c3_512   (r1 = r1 + r2)
	add_opcode(0x45, 0x0B, dataType, &inst[n]);
	set_destination(1, VX_ENABLE, 0, &inst[n]);
	set_evis(0, 7, (dataType | (dataType << 3)), &inst[n]);
	set_tempreg(0, 1, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 2, VX_SWIZZLE, 0, &inst[n]);
	set_source(2, 2, VX_SWIZZLE, 0x4, false, false, 0, &inst[n]);
	n += 4;

	// img_store.u8 r1, c2, r0.xy, r1
	add_opcode(0x7A, 0, dataType, &inst[n]);
	set_evis(0, (get_pixel(dataType) + 1) / numShaderCores - 1, 1, &inst[n]);
	set_uniform(0, 1, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	set_tempreg(2, 1, VX_SWIZZLE, 0, &inst[n]);
	n += 4;

	return ShaderInfo{n, 3}; // RegCount 3 (from _ProgramPPUInstruction)
}

// Build a real per-element add: OutImage = InImage + InImage. img_load/img_store
// stay EVIS (the copy probe proved they round-trip per pixel); between them a
// base ALU ADD (opcode 0x01, INST_OPCODE_ADD) with U8 operand type (INST_TYPE
// U8 = 7) doubles each byte in place -- out[pixel] = 2*in[pixel] mod 256. This
// is the template for arbitrary per-element ops: swap 0x01 for MUL 0x03,
// IADDSAT 0x3B, AND 0x5D, etc. Opcodes from the etnaviv rnndb isa.xml.
inline ShaderInfo build_add_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	unsigned n = 0;

	// img_load.u8 r1, c0, r0.xy
	add_opcode(0x79, 0, dataType, &inst[n]);
	set_destination(1, VX_ENABLE, 0, &inst[n]);
	set_evis(0, get_pixel(dataType), 1, &inst[n]);
	set_uniform(0, 0, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	n += 4;

	// add.u8 r1, r1, r1  (base ALU ADD: dst := src0 + src2, so src2 holds the
	// second operand; src1 is unused by ADD -- per isa.xml. No EVIS.)
	add_opcode(0x01, 0, dataType, &inst[n]);
	set_destination(1, VX_ENABLE, 0, &inst[n]);
	set_tempreg(0, 1, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(2, 1, VX_SWIZZLE, 0, &inst[n]);
	n += 4;

	// img_store.u8 r1, c1, r0.xy, r1
	add_opcode(0x7A, 0, dataType, &inst[n]);
	set_evis(0, (get_pixel(dataType) + 1) / numShaderCores - 1, 1, &inst[n]);
	set_uniform(0, 1, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	set_tempreg(2, 1, VX_SWIZZLE, 0, &inst[n]);
	n += 4;

	return ShaderInfo{n, 2};
}

// Build a TWO-INPUT add: OutImage = InA + InB, per element. A second img_load
// reads input B from image-descriptor uniform c2 (the dispatch binds it at
// state 0xD808); the base ALU ADD then does r1 = r1(A) + r2(B). Unlike
// build_add_shader (which adds A to itself), this reads a genuine second image.
// RegCount 3 (r0 coords, r1 = A, r2 = B).
inline ShaderInfo build_add2_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	unsigned n = 0;

	// img_load.u8 r1, c0, r0.xy   (input A)
	add_opcode(0x79, 0, dataType, &inst[n]);
	set_destination(1, VX_ENABLE, 0, &inst[n]);
	set_evis(0, get_pixel(dataType), 1, &inst[n]);
	set_uniform(0, 0, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	n += 4;

	// img_load.u8 r2, c2, r0.xy   (input B -- second image descriptor)
	add_opcode(0x79, 0, dataType, &inst[n]);
	set_destination(2, VX_ENABLE, 0, &inst[n]);
	set_evis(0, get_pixel(dataType), 1, &inst[n]);
	set_uniform(0, 2, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	n += 4;

	// add.u8 r1, r1, r2   (dst := src0 + src2 = A + B)
	add_opcode(0x01, 0, dataType, &inst[n]);
	set_destination(1, VX_ENABLE, 0, &inst[n]);
	set_tempreg(0, 1, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(2, 2, VX_SWIZZLE, 0, &inst[n]);
	n += 4;

	// img_store.u8 r1, c1, r0.xy, r1
	add_opcode(0x7A, 0, dataType, &inst[n]);
	set_evis(0, (get_pixel(dataType) + 1) / numShaderCores - 1, 1, &inst[n]);
	set_uniform(0, 1, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	set_tempreg(2, 1, VX_SWIZZLE, 0, &inst[n]);
	n += 4;

	return ShaderInfo{n, 3};
}

// Build a saturating per-element add: OutImage = saturate(InImage + InImage),
// i.e. out = min(2*in, 255) for u8. Identical to build_add_shader but with the
// IADDSAT opcode (0x3B) instead of ADD -- so 200+200 clamps to 255 rather than
// wrapping to 144. Same operand convention as ADD (dst := src0 + src2).
inline ShaderInfo build_addsat_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	unsigned n = 0;

	// img_load.u8 r1, c0, r0.xy
	add_opcode(0x79, 0, dataType, &inst[n]);
	set_destination(1, VX_ENABLE, 0, &inst[n]);
	set_evis(0, get_pixel(dataType), 1, &inst[n]);
	set_uniform(0, 0, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	n += 4;

	// iaddsat.u8 r1, r1, r1  (saturating add; dst := src0 + src2, clamped)
	add_opcode(0x3B, 0, dataType, &inst[n]);
	set_destination(1, VX_ENABLE, 0, &inst[n]);
	set_tempreg(0, 1, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(2, 1, VX_SWIZZLE, 0, &inst[n]);
	n += 4;

	// img_store.u8 r1, c1, r0.xy, r1
	add_opcode(0x7A, 0, dataType, &inst[n]);
	set_evis(0, (get_pixel(dataType) + 1) / numShaderCores - 1, 1, &inst[n]);
	set_uniform(0, 1, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	set_tempreg(2, 1, VX_SWIZZLE, 0, &inst[n]);
	n += 4;

	return ShaderInfo{n, 2};
}

// Build a bitwise-AND kernel: out = in & <immediate mask>. dst := src0 & src2
// with src2 a u32 immediate (SRC2_AMODE = 4/u32; passing the instruction
// dataType here instead gives an undefined AMODE and the AND silently becomes
// identity -- that bug cost a flash cycle). Per the ISA, an immediate is a
// 20-bit value broadcast to the 4 vector COMPONENTS, not sub-byte; so under u8
// SIMD (16 byte-lanes, consecutive pixels i..i+3 = the 4 bytes of a component)
// a small mask like 0x0F only masks the (i%4)==0 byte-lane and zeroes the rest:
// out[i] = (i%4==0) ? in[i]&0x0F : 0. A per-byte-uniform mask needs a uniform
// register (0x0F0F0F0F) or a second image full of the constant, not an immediate.
inline ShaderInfo build_and_imm_shader(uint32_t inst[16], uint32_t imm, uint32_t dataType = 0x7,
									   uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	unsigned n = 0;

	// img_load.u8 r1, c0, r0.xy
	add_opcode(0x79, 0, dataType, &inst[n]);
	set_destination(1, VX_ENABLE, 0, &inst[n]);
	set_evis(0, get_pixel(dataType), 1, &inst[n]);
	set_uniform(0, 0, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	n += 4;

	// and.u8 r1, r1, #imm  (dst := src0 & src2; src2 = u32 immediate, AMODE 4)
	add_opcode(0x5D, 0, dataType, &inst[n]);
	set_destination(1, VX_ENABLE, 0, &inst[n]);
	set_tempreg(0, 1, VX_SWIZZLE, 0, &inst[n]);
	set_immediate(2, imm, 2 /* u32: AMODE = type<<1 = 4 */, &inst[n]);
	n += 4;

	// img_store.u8 r1, c1, r0.xy, r1
	add_opcode(0x7A, 0, dataType, &inst[n]);
	set_evis(0, (get_pixel(dataType) + 1) / numShaderCores - 1, 1, &inst[n]);
	set_uniform(0, 1, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	set_tempreg(2, 1, VX_SWIZZLE, 0, &inst[n]);
	n += 4;

	return ShaderInfo{n, 2};
}

// AND with a fixed 0x0F mask, matching the (inst, dataType, cores) builder
// signature the test harness expects.
inline ShaderInfo build_and_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	return build_and_imm_shader(inst, 0x0F, dataType, numShaderCores);
}

// Build a copy kernel: OutImage = InImage (img_load -> img_store, no arithmetic
// in between). The simplest EVIS kernel -- a probe for whether the per-pixel
// load/store path round-trips cleanly, independent of any compute op.
inline ShaderInfo build_copy_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	unsigned n = 0;

	// img_load.u8 r1, c0, r0.xy
	add_opcode(0x79, 0, dataType, &inst[n]);
	set_destination(1, VX_ENABLE, 0, &inst[n]);
	set_evis(0, get_pixel(dataType), 1, &inst[n]);
	set_uniform(0, 0, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	n += 4;

	// img_store.u8 r1, c1, r0.xy, r1
	add_opcode(0x7A, 0, dataType, &inst[n]);
	set_evis(0, (get_pixel(dataType) + 1) / numShaderCores - 1, 1, &inst[n]);
	set_uniform(0, 1, VX_SWIZZLE, 0, &inst[n]);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, &inst[n]);
	set_tempreg(2, 1, VX_SWIZZLE, 0, &inst[n]);
	n += 4;

	return ShaderInfo{n, 2};
}

} // namespace ppu
