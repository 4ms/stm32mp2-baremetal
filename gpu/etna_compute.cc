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

// This is a command stream for the FE that loads and runs a shader over an
// input image, writing an output image. It is the fixed 64x6 sequence extracted
// from the vendor's _ProgramPPUCommand; emit_ppu_dispatch() below uses it as a
// TEMPLATE and patches the size/address/count slots so it works for any shader
// and image size (the vendor computes the group counts from width/height, so
// varying size is exactly what the dispatch is built to do).

// clang-format off
static const uint32_t kPpuDispatchTemplate[118] = {
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

// Template patch slots (dword offsets), verified against the extracted sequence.
enum : unsigned {
	kInAddr = 7,		// input image base address
	kInStride = 8,		// input stride (bytes)
	kInDims = 9,		// (height << 16) | width
	kOutAddr = 13,		// output image base address
	kOutStride = 14,	// output stride
	kOutDims = 15,		// (height << 16) | width
	kRegCount = 43,		// shader temp register count
	kInstCount4 = 51,	// InstCount / 4  (== number of instructions)
	kInstAddr = 53,		// shader binary base address
	kInstCount4m1 = 59, // InstCount / 4 - 1
	kGroupCountX = 95,	// groupCountX - 1
	kGroupCountY = 96,	// groupCountY - 1
	// Second input image descriptor -- uniform c2, the first 4 words of the
	// 0xD808 block (unused by single-input/copy shaders; the dp2x8 coefficients
	// otherwise). A two-input shader's 2nd img_load reads c2.
	kInBAddr = 19,	 // input B base address
	kInBStride = 20, // input B stride
	kInBDims = 21,	 // input B (height << 16) | width
	kInBFmt = 22,	 // input B format (same as input A's 0xD803)
	// Third input image descriptor -- uniform c3, next 4 words of the 0xD808
	// block (0xD80C..0xD80F). Used by three-input kernels (e.g. alpha blend).
	kInCAddr = 23,
	kInCStride = 24,
	kInCDims = 25,
	kInCFmt = 26,
};

constexpr uint32_t kImgFormat = 0x444051F0; // u8 image format word (from the template)

// Emit the PPU dispatch for `shader` over a `width` x `height` u8 image. The
// group counts are derived from the size the way the vendor's _ProgramPPUCommand
// does (globalScale 4x1), so this handles any size; everything else (USC config,
// uniforms, format, kick, drain) is the fixed template.
static void emit_ppu_dispatch(CmdStream &cs,
							  uint32_t in_addr,
							  uint32_t out_addr,
							  uint32_t shader_addr,
							  uint32_t inst_dwords,
							  uint32_t reg_count,
							  uint32_t width,
							  uint32_t height,
							  uint32_t in_b_addr = 0,
							  uint32_t in_c_addr = 0)
{
	const uint32_t stride = width; // u8: 1 byte per pixel
	const uint32_t dims = (height << 16) | width;
	const uint32_t group_x = (width + 3) / 4; // globalScaleX = 4
	const uint32_t group_y = height;		  // globalScaleY = 1

	for (unsigned i = 0; i < 118; i++) {
		uint32_t v = kPpuDispatchTemplate[i];
		// clang-format off
		switch (i) {
			case kInAddr:      v = in_addr; break;
			case kInStride:    v = stride; break;
			case kInDims:      v = dims; break;
			case kOutAddr:     v = out_addr; break;
			case kOutStride:   v = stride; break;
			case kOutDims:     v = dims; break;
			case kRegCount:    v = reg_count; break;
			case kInstCount4:  v = inst_dwords / 4; break;
			case kInstAddr:    v = shader_addr; break;
			case kInstCount4m1: v = inst_dwords / 4 - 1; break;
			case kGroupCountX: v = group_x - 1; break;
			case kGroupCountY: v = group_y - 1; break;
		}
		// clang-format on
		// Second input image (uniform c2), only for two-input shaders. Patches
		// the first 4 words of the 0xD808 block into a real image descriptor.
		if (in_b_addr) {
			switch (i) {
			case kInBAddr:   v = in_b_addr; break;
			case kInBStride: v = stride; break;
			case kInBDims:   v = dims; break;
			case kInBFmt:    v = kImgFormat; break;
			}
		}
		// Third input image (uniform c3), only for three-input shaders.
		if (in_c_addr) {
			switch (i) {
			case kInCAddr:   v = in_c_addr; break;
			case kInCStride: v = stride; break;
			case kInCDims:   v = dims; break;
			case kInCFmt:    v = kImgFormat; break;
			}
		}
		cs.emit(v);
	}
	// No emit_pe_drain: the template already ends with the vendor's drain;
	// submit() adds the ring's event + wait + link trailer.
}

Kernel make_kernel(Gpu &gpu, ShaderBuilder build)
{
	uint32_t inst[32];
	auto si = build(inst, 0x7, 2); // dataType u8, NumShaderCores 2
	Kernel k;
	k.binary = gpu.alloc(si.inst_dwords * 4);
	if (!k.binary)
		return k; // empty
	auto p = static_cast<uint32_t *>(k.binary.map());
	for (unsigned i = 0; i < si.inst_dwords; i++)
		p[i] = inst[i];
	k.binary.cpu_fini(RelocWrite);
	k.inst_dwords = si.inst_dwords;
	k.reg_count = si.reg_count;
	return k;
}

// Shared implementation for all input arities; in1/in2 are null when unused.
static bool run_kernel(Gpu &gpu, const Kernel &k, const Bo &out, const Bo &in0, const Bo *in1, const Bo *in2,
					   uint32_t width, uint32_t height)
{
	auto cs = gpu.new_cmd_stream(256);
	emit_ppu_dispatch(cs, in0.gpu_addr(), out.gpu_addr(), k.binary.gpu_addr(), k.inst_dwords, k.reg_count, width,
					  height, in1 ? in1->gpu_addr() : 0, in2 ? in2->gpu_addr() : 0);
	return gpu.submit_and_wait(cs);
}

bool compute(Gpu &gpu, const Kernel &k, const Bo &out, const Bo &in0, uint32_t width, uint32_t height)
{
	return run_kernel(gpu, k, out, in0, nullptr, nullptr, width, height);
}

bool compute(Gpu &gpu, const Kernel &k, const Bo &out, const Bo &in0, const Bo &in1, uint32_t width,
			 uint32_t height)
{
	return run_kernel(gpu, k, out, in0, &in1, nullptr, width, height);
}

bool compute(Gpu &gpu, const Kernel &k, const Bo &out, const Bo &in0, const Bo &in1, const Bo &in2,
			 uint32_t width, uint32_t height)
{
	return run_kernel(gpu, k, out, in0, &in1, &in2, width, height);
}

// Run an arbitrary kernel over a width x height u8 image: build the shader,
// fill the input with fill(i), dispatch, and verify each output byte equals
// expect(i). Dumps the first 16 in/out bytes on the first mismatch so the
// actual per-element behavior is visible.
using BuildFn = ppu::ShaderInfo (*)(uint32_t *, uint32_t, uint32_t);
using ByteFn = uint8_t (*)(uint32_t);

static bool run(Gpu &gpu, const char *name, uint32_t width, uint32_t height, BuildFn build, ByteFn fill, ByteFn expect)
{
	uint32_t n = width * height;
	Bo in = gpu.alloc(n), out = gpu.alloc(n);
	Kernel k = make_kernel(gpu, build);
	if (!in || !out || !k)
		return false;

	auto ib = static_cast<uint8_t *>(in.map());
	for (uint32_t i = 0; i < n; i++)
		ib[i] = fill(i);
	in.cpu_fini(RelocWrite);

	auto ob = static_cast<uint8_t *>(out.map());
	for (uint32_t i = 0; i < n; i++)
		ob[i] = 0xEE; // poison
	out.cpu_fini(RelocWrite);

	auto start = read_cntpct();
	if (!compute(gpu, k, out, in, width, height))
		return false;
	uint32_t ticks = read_cntpct() - start;

	out.cpu_prep(RelocRead);
	in.cpu_prep(RelocRead);
	for (uint32_t i = 0; i < n; i++) {
		if (ob[i] != expect(i)) {
			print("ERROR: ", name, " wrong at byte ", int(i));
			print(": got ", int(ob[i]), " expected ", int(expect(i)), "\n");
			print("in [0..15]:");
			for (int j = 0; j < 16 && j < (int)n; j++)
				print(" ", int(ib[j]));
			print("\n  out[0..15]:");
			for (int j = 0; j < 16 && j < (int)n; j++)
				print(" ", int(ob[j]));
			print("\n");
			return false;
		}
	}
	print("GPU ", name, " over ", int(width), "x", int(height));
	print(" (", int(n), " bytes) in ", ticks, " ticks -- verified. \\o/\n");
	return true;
}

// Like run(), but two input images: fill a with fillA(i), b with fillB(i),
// dispatch a two-input kernel, and verify out[i] == expect(i).
static bool run2(Gpu &gpu, const char *name, uint32_t width, uint32_t height, BuildFn build, ByteFn fillA,
				 ByteFn fillB, ByteFn expect)
{
	uint32_t n = width * height;
	Bo a = gpu.alloc(n), b = gpu.alloc(n), out = gpu.alloc(n);
	Kernel k = make_kernel(gpu, build);
	if (!a || !b || !out || !k)
		return false;

	auto ab = static_cast<uint8_t *>(a.map());
	auto bb = static_cast<uint8_t *>(b.map());
	for (uint32_t i = 0; i < n; i++) {
		ab[i] = fillA(i);
		bb[i] = fillB(i);
	}
	a.cpu_fini(RelocWrite);
	b.cpu_fini(RelocWrite);

	auto ob = static_cast<uint8_t *>(out.map());
	for (uint32_t i = 0; i < n; i++)
		ob[i] = 0xEE; // poison
	out.cpu_fini(RelocWrite);

	auto start = read_cntpct();
	if (!compute(gpu, k, out, a, b, width, height))
		return false;
	uint32_t ticks = read_cntpct() - start;

	out.cpu_prep(RelocRead);
	a.cpu_prep(RelocRead);
	b.cpu_prep(RelocRead);
	for (uint32_t i = 0; i < n; i++) {
		if (ob[i] != expect(i)) {
			print("ERROR: ", name, " wrong at byte ", int(i));
			print(": got ", int(ob[i]), " expected ", int(expect(i)), "\n  a[0..15]:");
			for (int j = 0; j < 16 && j < (int)n; j++)
				print(" ", int(ab[j]));
			print("\n  b[0..15]:");
			for (int j = 0; j < 16 && j < (int)n; j++)
				print(" ", int(bb[j]));
			print("\n  out[0..15]:");
			for (int j = 0; j < 16 && j < (int)n; j++)
				print(" ", int(ob[j]));
			print("\n");
			return false;
		}
	}
	print("GPU ", name, " over ", int(width), "x", int(height));
	print(" (", int(n), " bytes) in ", ticks, " ticks -- verified. \\o/\n");
	return true;
}

// Three-input variant (a, b, c), shader up to 8 instructions (inst[32]).
static bool run3(Gpu &gpu, const char *name, uint32_t width, uint32_t height, BuildFn build, ByteFn fillA,
				 ByteFn fillB, ByteFn fillC, ByteFn expect)
{
	uint32_t n = width * height;
	Bo a = gpu.alloc(n), b = gpu.alloc(n), c = gpu.alloc(n), out = gpu.alloc(n);
	Kernel k = make_kernel(gpu, build);
	if (!a || !b || !c || !out || !k)
		return false;

	auto ab = static_cast<uint8_t *>(a.map());
	auto bb = static_cast<uint8_t *>(b.map());
	auto cb = static_cast<uint8_t *>(c.map());
	for (uint32_t i = 0; i < n; i++) {
		ab[i] = fillA(i);
		bb[i] = fillB(i);
		cb[i] = fillC(i);
	}
	a.cpu_fini(RelocWrite);
	b.cpu_fini(RelocWrite);
	c.cpu_fini(RelocWrite);

	auto ob = static_cast<uint8_t *>(out.map());
	for (uint32_t i = 0; i < n; i++)
		ob[i] = 0xEE; // poison
	out.cpu_fini(RelocWrite);

	auto start = read_cntpct();
	if (!compute(gpu, k, out, a, b, c, width, height))
		return false;
	uint32_t ticks = read_cntpct() - start;

	out.cpu_prep(RelocRead);
	a.cpu_prep(RelocRead);
	b.cpu_prep(RelocRead);
	c.cpu_prep(RelocRead);
	for (uint32_t i = 0; i < n; i++) {
		if (ob[i] != expect(i)) {
			print("ERROR: ", name, " wrong at byte ", int(i));
			print(": got ", int(ob[i]), " expected ", int(expect(i)), "\n  a:");
			for (int j = 0; j < 12 && j < (int)n; j++)
				print(" ", int(ab[j]));
			print("\n  b:");
			for (int j = 0; j < 12 && j < (int)n; j++)
				print(" ", int(bb[j]));
			print("\n  c:");
			for (int j = 0; j < 12 && j < (int)n; j++)
				print(" ", int(cb[j]));
			print("\n  out:");
			for (int j = 0; j < 12 && j < (int)n; j++)
				print(" ", int(ob[j]));
			print("\n");
			return false;
		}
	}
	print("GPU ", name, " over ", int(width), "x", int(height));
	print(" (", int(n), " bytes) in ", ticks, " ticks -- verified. \\o/\n");
	return true;
}

// A per-element gradient and its identity expectation, for the copy probe.
static uint8_t grad(uint32_t i)
{
	return i & 0x7F;
}

bool compute_test(Gpu &gpu)
{
	// Copy (out = in) with a per-element gradient, at several sizes.
	struct {
		uint32_t w, h;
	} sizes[] = {{64, 6}, {128, 32}, {256, 64}, {32, 4}};
	for (auto &s : sizes)
		if (!run(gpu, "copy", s.w, s.h, ppu::build_copy_shader, grad, grad))
			return false;

	// Real per-element add (base ALU ADD.u8, out = in + in) with a FULL-range
	// gradient -- verifies both per-element arithmetic and u8 wraparound.
	if (!run(
			gpu,
			"add(out=in+in)",
			128,
			32,
			ppu::build_add_shader,
			[](uint32_t i) -> uint8_t { return i & 0xFF; },
			[](uint32_t i) -> uint8_t { return (i & 0xFF) * 2; }))
		return false;

	// Saturating add (IADDSAT.u8, out = min(in+in, 255)) -- same gradient, but
	// now high values clamp instead of wrapping (e.g. 200 -> 255, not 144).
	if (!run(
			gpu,
			"addsat(min(in+in,255))",
			128,
			32,
			ppu::build_addsat_shader,
			[](uint32_t i) -> uint8_t { return i & 0xFF; },
			[](uint32_t i) -> uint8_t { uint32_t s = (i & 0xFF) * 2; return s > 255 ? 255 : s; }))
		return false;

	// TWO-INPUT add: OutImage = A + B, reading a second input image from
	// descriptor c2. A = ramp up (i), B = ramp down (255-i), so A+B = 255
	// everywhere -- a strong check: any index misalignment or ignored B breaks
	// the constant. Proves the shader reads a genuine second image. (Runs before
	// the AND test so the two-input path is exercised regardless of it.)
	if (!run2(
			gpu,
			"add2(A+B, ramp+inv=255)",
			128,
			32,
			ppu::build_add2_shader,
			[](uint32_t i) -> uint8_t { return i & 0xFF; },
			[](uint32_t i) -> uint8_t { return 255 - (i & 0xFF); },
			[](uint32_t) -> uint8_t { return 255; }))
		return false;

	// ADDITIVE (saturating) blend of two images: out = saturate(A + B). A =
	// gradient, B = constant 128, so out clips to 255 once A >= 128. A real
	// compositing mode ("linear dodge") over two distinct images.
	if (!run2(
			gpu,
			"blend-add(sat(A+B))",
			128,
			32,
			ppu::build_addsat2_shader,
			[](uint32_t i) -> uint8_t { return i & 0xFF; },
			[](uint32_t) -> uint8_t { return 128; },
			[](uint32_t i) -> uint8_t { uint32_t s = (i & 0xFF) + 128; return s > 255 ? 255 : s; }))
		return false;

	// Bitwise AND with u32 immediate 0x0F. The immediate broadcasts to the 4
	// vector COMPONENTS, so under u8 SIMD it masks only the (i%4)==0 byte-lane
	// and zeroes the other three: out[i] = (i%4==0) ? in[i]&0x0F : 0.
	if (!run(
			gpu,
			"and(in & 0x0F, imm)",
			64,
			6,
			ppu::build_and_shader,
			[](uint32_t i) -> uint8_t { return i & 0xFF; },
			[](uint32_t i) -> uint8_t { return (i % 4 == 0) ? ((i & 0xFF) & 0x0F) : 0; }))
		return false;

	// The vendor flop-reset kernel, constant input -- its only verifiable case:
	// dp2x8 is a dot-product (out[j] = sum of src0[k]*src1[k] over a pair, i.e.
	// sums of squares here), so it only lands on 2*in when every input is equal.
	if (!run(
			gpu,
			"flop-reset(dp2x8, const in)",
			64,
			6,
			ppu::build_dp2x8_shader,
			[](uint32_t) -> uint8_t { return 0x01; },
			[](uint32_t) -> uint8_t { return 0x02; }))
		return false;

	// PROBE (last, so a scalar-multiply failure doesn't block verified tests):
	// per-byte multiply out = (A*B)&0xFF. A = gradient, B = constant 3. If this
	// verifies, IMULLO is per-byte SIMD and fractional alpha is reachable; if it
	// fails, the dump shows whether it's scalar (broadcast of A[0]*3).
	if (!run2(
			gpu,
			"PROBE mul2((A*B)&0xFF)",
			64,
			6,
			ppu::build_mul2_shader,
			[](uint32_t i) -> uint8_t { return i & 0xFF; },
			[](uint32_t) -> uint8_t { return 3; },
			[](uint32_t i) -> uint8_t { return ((i & 0xFF) * 3) & 0xFF; }))
		return false;

	// PROBE: high-half multiply out = mul_hi(A, B). B = 128, so if u8 mul_hi is
	// (A*B)>>8 then out = (A*128)>>8 = A>>1. Confirms the primitive for a*alpha.
	if (!run2(
			gpu,
			"PROBE mulhi2(mul_hi(A,B))",
			64,
			6,
			ppu::build_mulhi2_shader,
			[](uint32_t i) -> uint8_t { return i & 0xFF; },
			[](uint32_t) -> uint8_t { return 128; },
			[](uint32_t i) -> uint8_t { return (i & 0xFF) >> 1; }))
		return false;

	// PROBE: bitwise NOT, out = ~in = 255 - in (needed for beta = 255 - alpha).
	if (!run(
			gpu,
			"PROBE not(~in)",
			64,
			6,
			ppu::build_not_shader,
			[](uint32_t i) -> uint8_t { return i & 0xFF; },
			[](uint32_t i) -> uint8_t { return 255 - (i & 0xFF); }))
		return false;

	// TRUE fractional alpha blend: out = mul_hi(a,alpha) + mul_hi(b,255-alpha).
	// a = ramp up, b = ramp down, alpha = ramp up (per-pixel). Verified exactly
	// against the same integer math on the CPU.
	if (!run3(
			gpu,
			"blend-lerp(a*A + b*(1-A))",
			128,
			32,
			ppu::build_blend_lerp_shader,
			[](uint32_t i) -> uint8_t { return i & 0xFF; },		   // a
			[](uint32_t i) -> uint8_t { return 255 - (i & 0xFF); }, // b
			[](uint32_t i) -> uint8_t { return i & 0xFF; },		   // alpha
			[](uint32_t i) -> uint8_t {
				uint32_t a = i & 0xFF, b = 255 - (i & 0xFF), al = i & 0xFF;
				return (uint8_t)(((a * al) >> 8) + ((b * (255 - al)) >> 8));
			}))
		return false;
	return true;
}

} // namespace etna
