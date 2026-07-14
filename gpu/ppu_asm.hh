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

// gckPPU_SetImmediate
inline void set_immediate(unsigned where, uint32_t value, uint32_t type, uint32_t *inst)
{
	switch (where) {
	case 0:
		setbits(inst[1], 20, 12, getbits(value, 8, 0));
		setbits(inst[1], 29, 22, getbits(value, 16, 9));
		setbits(inst[1], 30, 30, getbit(value, 17));
		setbits(inst[1], 31, 31, getbit(value, 18));
		setbits(inst[2], 2, 0, getbit(value, 19) | (type << 1));
		setbits(inst[2], 5, 3, 0x7);
		break;
	case 1:
		setbits(inst[2], 15, 7, getbits(value, 8, 0));
		setbits(inst[2], 24, 17, getbits(value, 16, 9));
		setbits(inst[2], 25, 25, getbit(value, 17));
		setbits(inst[2], 26, 26, getbit(value, 18));
		setbits(inst[2], 29, 27, getbit(value, 19) | (type << 1));
		setbits(inst[3], 2, 0, 0x7);
		break;
	case 2:
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

// Build the flop-reset compute kernel: OutImage = InImage + InImage, in EVIS
// (img_load x2, dp2x8 add, img_store). Mirrors _ProgramPPUInstruction exactly.
// Writes 16 dwords into `inst`, returns the count (16). RegCount is 3.
// dataType 0x7 = u8, numShaderCores from the feature DB (2 on our chip).
inline unsigned build_add_shader(uint32_t inst[16], uint32_t dataType = 0x7, uint32_t numShaderCores = 2)
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

	return n;
}

} // namespace ppu
