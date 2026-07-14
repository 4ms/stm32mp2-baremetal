#include "aarch64/system_reg.hh"
#include "etna.hh"
#include "ppu_asm.hh"
#include "print/print.hh"

// Compute (PPU / unified-shader) dispatch, ported from the gcnano vendor
// driver's flop_reset (_ProgramPPUInstruction + _ProgramPPUCommand, dual
// MIT/GPL; used under MIT). See gpu/reference/gcnano_flop_reset/.
//
// It runs a shader "OutImage = InImage + InImage" (EVIS img_load x2 -> dp2x8
// add -> img_store) over a 64x6 u8 image. The dispatch command sequence below
// was extracted verbatim from _ProgramPPUCommand for OUR exact chip
// (GCNANOULTRA31_VIP2, model 0x8000 rev 0x6205 customer 0x15), with every
// parameter resolved (dataType 0x7, NumShaderCores 2, groupSize 1x1, global
// scale 4x1, groupCount 16x6, RegCount 3, InstCount 16). The three buffer
// addresses are patched in at runtime (dword offsets 7/13/53). 0xBADABEEB at
// state 0x0248 is the PPU dispatch "kick" (compute's analog of RS_KICKER).

namespace etna
{

// This is a command stream for the FE, which runs a shader on an input
// We replace certain words with the address of our input buffer, output buffer,
// and shader code (which was generated in ppu_asm.hh).

// clang-format off
static const uint32_t kPpuDispatch[118] = {
	0x08010E13, 0x00000002, 0x08010E02, 0x00000701, 0x48000000, 0x00000701, 0x0804D800, 0x00000000, // [7]=IN
	0x00000040, 0x00060040, 0x444051F0, 0xFFFFFFFF, 0x0804D804, 0x00000000, 0x00000040, 0x00060040, // [13]=OUT
	0x444051F0, 0xFFFFFFFF, 0x0810D808, 0x55555555, 0x00000000, 0x01234567, 0x89ABCDEF, 0x55555555,
	0x01234567, 0x89ABCDEF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0x08010240, 0x02000002, 0x0801022C, 0x0000001F,
	0x08010420, 0x00000000, 0x08010403, 0x00000003, 0x08010416, 0x00000000, 0x08010409, 0x00000000,
	0x0801021F, 0x00000000, 0x08010424, 0x00000004, 0x0801040A, 0x00000000, 0x08015580, 0x00000002, // [53]=INST
	0x0801021A, 0x00000001, 0x08010425, 0x00000003, 0x08010402, 0x00001F01, 0x08010228, 0x00000000,
	0x080102AA, 0x00000000, 0x08010E07, 0x00000000, 0x0801040C, 0x00000000, 0x08010201, 0x00000001,
	0x08010E22, 0x00000000, 0x08010412, 0x00000000, 0x08010240, 0x03000002, 0x08010249, 0x00000000,
	0x08010247, 0x00000001, 0x0801024B, 0x00000000, 0x0801024D, 0x00000000, 0x0801024F, 0x00000000,
	0x08010256, 0x00000004, 0x08010257, 0x00000001, 0x08010258, 0x00000000, 0x08060250, 0x0000000F,
	0x00000005, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x000003FF, 0x00000000, 0x08010248, 0xBADABEEB, // kick
	0x08010E03, 0x00000C20, 0x08010E02, 0x00000701, 0x48000000, 0x00000701, 0x08010E03, 0x00000C23,
	0x08010E03, 0x00000C23, 0x08010594, 0x00000001, 0x08010E03, 0x00000C23,
};
// clang-format on
constexpr unsigned kInIdx = 7, kOutIdx = 13, kInstIdx = 53;

bool compute_add_test(Gpu &gpu)
{
	constexpr uint32_t W = 64, H = 6;
	constexpr uint32_t N = W * H; // 384 bytes, u8

	Bo in = gpu.alloc(N);
	Bo out = gpu.alloc(N);
	Bo shader = gpu.alloc(16 * 4); // 4 instructions x 4 dwords
	if (!in || !out || !shader)
		return false;

	// Build the "out = in + in" kernel into the shader buffer.
	uint32_t inst[16];
	ppu::build_add_shader(inst, 0x7, 2);
	auto sp = static_cast<uint32_t *>(shader.map());
	for (unsigned i = 0; i < 16; i++)
		sp[i] = inst[i];
	shader.cpu_fini(RelocWrite);

	// Input image: every byte = 0x01 (PPU_IMAGE_DATA).
	auto ip = static_cast<uint32_t *>(in.map());
	for (uint32_t i = 0; i < N / 4; i++)
		ip[i] = 0x01010101;
	in.cpu_fini(RelocWrite);

	// Poison the output so a partial/failed dispatch is obvious.
	auto op = static_cast<uint32_t *>(out.map());
	for (uint32_t i = 0; i < N / 4; i++)
		op[i] = 0xDEADBEEF;
	out.cpu_fini(RelocWrite);

	// Emit the dispatch, patching in the buffer addresses. The sequence already
	// ends with the vendor's own drain, so no emit_pe_drain -- submit() just
	// adds the ring's event + wait + link trailer.
	auto cs = gpu.new_cmd_stream(256);
	for (unsigned i = 0; i < 118; i++) {
		uint32_t v = kPpuDispatch[i];
		if (i == kInIdx)
			v = in.gpu_addr();
		else if (i == kOutIdx)
			v = out.gpu_addr();
		else if (i == kInstIdx)
			v = shader.gpu_addr();
		cs.emit(v);
	}

	auto start = read_cntpct();
	if (!gpu.submit_and_wait(cs))
		return false;
	print("PPU compute dispatch in ", (uint32_t)(read_cntpct() - start), " ticks\n");

	out.cpu_prep(RelocRead);
	unsigned wrong = 0, first = 0;
	for (uint32_t i = 0; i < N / 4; i++) {
		if (op[i] != 0x02020202) {
			if (wrong == 0)
				first = i;
			wrong++;
		}
	}
	if (wrong) {
		print("ERROR: compute wrong at ", int(wrong), " of ", int(N / 4), " words. [", int(first));
		print("] = 0x", Hex{op[first]}, " (expected 0x02020202)\n");
		return false;
	}
	print("GPU compute shader: out = in + in over ", int(N), " bytes -- verified. \\o/\n");
	return true;
}

} // namespace etna
