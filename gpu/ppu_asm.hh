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
constexpr uint32_t VX_ENABLE = 0xF;									// all 4 components
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
inline void set_source(unsigned where,
					   uint32_t address,
					   uint32_t swizzle,
					   uint32_t type,
					   bool negate,
					   bool absolute,
					   uint32_t relative,
					   uint32_t *inst)
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
// Opcodes from the etnaviv rnndb isa.xml.
struct ShaderInfo {
	unsigned inst_dwords; // total dwords (4 per instruction)
	unsigned reg_count;	  // temp registers the shader uses
};

// -----------------------------------------------------------------------------
//  Kernel-building helpers
// -----------------------------------------------------------------------------
// A kernel is: load 1..3 input images -> a few ALU ops -> store 1 output image.
// Each helper emits ONE instruction (4 dwords) at `I`. Images bind to descriptor
// uniforms: c0 = input 0, c1 = output, c2 = input 1, c3 = input 2. Register r0
// holds the per-pixel coordinate (hardware-provided); r1.. are temporaries.
// Opcodes/types are from the etnaviv rnndb isa.xml; note the two operand
// conventions -- ADD/AND/logic use src0+src2, the multiply family uses src0*src1.

// img_load.u8 dst_reg <- image at descriptor uniform `slot`, at coord r0.
inline void emit_img_load(uint32_t *I, uint32_t dst_reg, uint32_t slot, uint32_t dataType)
{
	add_opcode(0x79, 0, dataType, I);
	set_destination(dst_reg, VX_ENABLE, 0, I);
	set_evis(0, get_pixel(dataType), 1, I);
	set_uniform(0, slot, VX_SWIZZLE, 0, I);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, I);
}

// img_store.u8 output image (descriptor uniform c1) <- src_reg, at coord r0.
inline void emit_img_store(uint32_t *I, uint32_t src_reg, uint32_t dataType, uint32_t cores)
{
	add_opcode(0x7A, 0, dataType, I);
	set_evis(0, (get_pixel(dataType) + 1) / cores - 1, 1, I);
	set_uniform(0, 1, VX_SWIZZLE, 0, I);
	set_tempreg(1, 0, vx_swizzle2(0, 1), 0, I);
	set_tempreg(2, src_reg, VX_SWIZZLE, 0, I);
}

// Two-source ALU op, dst := src0 <op> src2 (ADD 0x01, IADDSAT 0x3B, AND 0x5D...).
inline void emit_alu(uint32_t *I, uint32_t opcode, uint32_t dst, uint32_t src0, uint32_t src2, uint32_t dataType)
{
	add_opcode(opcode, 0, dataType, I);
	set_destination(dst, VX_ENABLE, 0, I);
	set_tempreg(0, src0, VX_SWIZZLE, 0, I);
	set_tempreg(2, src2, VX_SWIZZLE, 0, I);
}

// Multiply-family op, dst := src0 * src1 (IMULLO 0x3C = low half, IMULHI 0x40 =
// high half / mul_hi). This family takes the 2nd operand in src1, NOT src2.
inline void emit_mul(uint32_t *I, uint32_t opcode, uint32_t dst, uint32_t src0, uint32_t src1, uint32_t dataType)
{
	add_opcode(opcode, 0, dataType, I);
	set_destination(dst, VX_ENABLE, 0, I);
	set_tempreg(0, src0, VX_SWIZZLE, 0, I);
	set_tempreg(1, src1, VX_SWIZZLE, 0, I);
}

// Unary op, dst := ~src2 (NOT 0x5F; ~x == 255 - x for u8).
inline void emit_not(uint32_t *I, uint32_t dst, uint32_t src2, uint32_t dataType)
{
	add_opcode(0x5F, 0, dataType, I);
	set_destination(dst, VX_ENABLE, 0, I);
	set_tempreg(2, src2, VX_SWIZZLE, 0, I);
}

// Two-source op with a u32 immediate 2nd operand, dst := src0 <op> #imm. The
// immediate broadcasts to the 4 vector components (see build_and_imm_shader).
inline void emit_alu_imm(uint32_t *I, uint32_t opcode, uint32_t dst, uint32_t src0, uint32_t imm, uint32_t dataType)
{
	add_opcode(opcode, 0, dataType, I);
	set_destination(dst, VX_ENABLE, 0, I);
	set_tempreg(0, src0, VX_SWIZZLE, 0, I);
	set_immediate(2, imm, 2 /* u32: AMODE = type<<1 = 4 */, I);
}

// =============================================================================
//  Kernels -- each builds a shader program into `inst`, returns its ShaderInfo.
//  Single-input first, then two-input, three-input, and the vendor dp2x8.
// =============================================================================

// out = in  (img_load -> img_store). The simplest kernel; also the probe that
// proved the per-pixel load/store path round-trips cleanly.
inline ShaderInfo build_copy_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	emit_img_load(&inst[0], 1, 0, dataType);			 // r1 <- in
	emit_img_store(&inst[4], 1, dataType, numShaderCores); // out <- r1
	return ShaderInfo{8, 2};
}

// out = in + in  (base ALU ADD, per byte, wraps mod 256). The template for any
// per-element op: swap ADD 0x01 for IADDSAT 0x3B, AND 0x5D, etc.
inline ShaderInfo build_add_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	emit_img_load(&inst[0], 1, 0, dataType);
	emit_alu(&inst[4], 0x01, 1, 1, 1, dataType); // r1 = r1 + r1
	emit_img_store(&inst[8], 1, dataType, numShaderCores);
	return ShaderInfo{12, 2};
}

// out = saturate(in + in) = min(2*in, 255)  (IADDSAT clamps instead of wrapping).
inline ShaderInfo build_addsat_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	emit_img_load(&inst[0], 1, 0, dataType);
	emit_alu(&inst[4], 0x3B, 1, 1, 1, dataType); // r1 = saturate(r1 + r1)
	emit_img_store(&inst[8], 1, dataType, numShaderCores);
	return ShaderInfo{12, 2};
}

// out = ~in = 255 - in  (NOT). Also used to make 255-alpha in the blend.
inline ShaderInfo build_not_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	emit_img_load(&inst[0], 1, 0, dataType);
	emit_not(&inst[4], 1, 1, dataType); // r1 = ~r1
	emit_img_store(&inst[8], 1, dataType, numShaderCores);
	return ShaderInfo{12, 2};
}

// out = in & <immediate mask>. Per the ISA an immediate is a 20-bit value
// broadcast to the 4 vector COMPONENTS (not sub-byte), so under u8 SIMD (16
// byte-lanes; consecutive pixels i..i+3 = the 4 bytes of a component) a small
// mask like 0x0F only masks the (i%4)==0 byte-lane and zeroes the rest:
// out[i] = (i%4==0) ? in[i]&0x0F : 0. A per-byte-uniform mask needs a uniform
// register (0x0F0F0F0F) or a constant-filled input image, not an immediate.
inline ShaderInfo
build_and_imm_shader(uint32_t inst[16], uint32_t imm, uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	emit_img_load(&inst[0], 1, 0, dataType);
	emit_alu_imm(&inst[4], 0x5D, 1, 1, imm, dataType); // r1 = r1 & #imm
	emit_img_store(&inst[8], 1, dataType, numShaderCores);
	return ShaderInfo{12, 2};
}

// AND with a fixed 0x0F mask (matches the (inst, dataType, cores) signature the
// test harness / make_kernel expect).
inline ShaderInfo build_and_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	return build_and_imm_shader(inst, 0x0F, dataType, numShaderCores);
}

// out = A + B, two input images (A=c0, B=c2), per byte (wraps).
inline ShaderInfo build_add2_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	emit_img_load(&inst[0], 1, 0, dataType);	 // r1 <- A
	emit_img_load(&inst[4], 2, 2, dataType);	 // r2 <- B
	emit_alu(&inst[8], 0x01, 1, 1, 2, dataType); // r1 = A + B
	emit_img_store(&inst[12], 1, dataType, numShaderCores);
	return ShaderInfo{16, 3};
}

// out = saturate(A + B), the "linear dodge" additive compositing blend.
inline ShaderInfo build_addsat2_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	emit_img_load(&inst[0], 1, 0, dataType);
	emit_img_load(&inst[4], 2, 2, dataType);
	emit_alu(&inst[8], 0x3B, 1, 1, 2, dataType); // r1 = saturate(A + B)
	emit_img_store(&inst[12], 1, dataType, numShaderCores);
	return ShaderInfo{16, 3};
}

// out = (A * B) & 0xFF, per-byte low-half multiply (IMULLO). Proved a per-byte
// integer multiply exists here despite the ISA marking IMULLO "scalar".
inline ShaderInfo build_mul2_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	emit_img_load(&inst[0], 1, 0, dataType);
	emit_img_load(&inst[4], 2, 2, dataType);
	emit_mul(&inst[8], 0x3C, 1, 1, 2, dataType); // r1 = (A * B) & 0xFF
	emit_img_store(&inst[12], 1, dataType, numShaderCores);
	return ShaderInfo{16, 3};
}

// out = mul_hi(A, B) = (A * B) >> 8, per-byte high-half multiply (IMULHI). The
// scale primitive behind the alpha blend (a*alpha >> 8).
inline ShaderInfo build_mulhi2_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	emit_img_load(&inst[0], 1, 0, dataType);
	emit_img_load(&inst[4], 2, 2, dataType);
	emit_mul(&inst[8], 0x40, 1, 1, 2, dataType); // r1 = mul_hi(A, B)
	emit_img_store(&inst[12], 1, dataType, numShaderCores);
	return ShaderInfo{16, 3};
}

// Fractional alpha blend: out = mul_hi(a, alpha) + mul_hi(b, 255-alpha)
//   ~= (a*alpha + b*(255-alpha)) / 256 -- a per-pixel lerp between images a (c0)
// and b (c2) by a per-pixel alpha image (c3). beta = 255-alpha via NOT (no 4th
// input). Both mul_hi terms are <=254 and their sum <=254, so ADD can't overflow.
inline ShaderInfo build_blend_lerp_shader(uint32_t inst[32], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 32; i++)
		inst[i] = 0;
	emit_img_load(&inst[0], 1, 0, dataType);	  // r1 <- a
	emit_img_load(&inst[4], 2, 2, dataType);	  // r2 <- b
	emit_img_load(&inst[8], 3, 3, dataType);	  // r3 <- alpha
	emit_not(&inst[12], 4, 3, dataType);		  // r4 = ~alpha = 255-alpha
	emit_mul(&inst[16], 0x40, 1, 1, 3, dataType); // r1 = mul_hi(a, alpha)
	emit_mul(&inst[20], 0x40, 2, 2, 4, dataType); // r2 = mul_hi(b, 255-alpha)
	emit_alu(&inst[24], 0x01, 1, 1, 2, dataType); // r1 = r1 + r2
	emit_img_store(&inst[28], 1, dataType, numShaderCores);
	return ShaderInfo{32, 5};
}

// The vendor flop-reset kernel: img_load x2 -> dp2x8 -> img_store, a faithful
// copy of _ProgramPPUInstruction. NOTE despite the vendor's comment this is NOT
// a per-element add: dp2x8 is a dot-product (out[j] = sum of src0[k]*src1[k]
// over a pair), which only looks like 2*in when every input is equal. Kept
// as-is because it's the known-good flop-toggle the vendor ships. Both loads
// read the SAME input (c0); the dp2x8's coefficients come from EVIS uniform c2.
inline ShaderInfo build_dp2x8_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
{
	for (unsigned i = 0; i < 16; i++)
		inst[i] = 0;
	emit_img_load(&inst[0], 1, 0, dataType); // r1 <- in
	emit_img_load(&inst[4], 2, 0, dataType); // r2 <- same in

	// dp2x8 r1, r1, r2, c2 (EVIS dot-product; not expressible via emit_* helpers)
	add_opcode(0x45, 0x0B, dataType, &inst[8]);
	set_destination(1, VX_ENABLE, 0, &inst[8]);
	set_evis(0, 7, (dataType | (dataType << 3)), &inst[8]);
	set_tempreg(0, 1, VX_SWIZZLE, 0, &inst[8]);
	set_tempreg(1, 2, VX_SWIZZLE, 0, &inst[8]);
	set_source(2, 2, VX_SWIZZLE, 0x4, false, false, 0, &inst[8]);

	emit_img_store(&inst[12], 1, dataType, numShaderCores);
	return ShaderInfo{16, 3};
}

} // namespace ppu
