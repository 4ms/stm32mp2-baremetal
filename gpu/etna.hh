#pragma once
#include "gpu_regs.hh"
#include "ppu_asm.hh" // ppu::ShaderInfo / build_*_shader (for Kernel/make_kernel)
#include <atomic>
#include <cstddef>
#include <cstdint>

// =============================================================================
//  etna.hh -- a baremetal GPU API for the STM32MP25 Vivante (GCNanoUltra31)
// =============================================================================
//
// This is the "middle seam" of the etnaviv stack, reimplemented for baremetal.
// In a Linux system the layering is:
//
//     Mesa gallium driver   (MIT)   -- builds command streams for draws/blits
//         |  calls etna_cmd_stream_emit() / _reloc() / etna_set_state()
//     libdrm etnaviv API    (MIT)   -- etna_device/gpu/pipe/bo/cmd_stream  <== THIS
//         |  DRM ioctls
//     kernel etnaviv driver (GPL)   -- power/clock/reset, MMU, submission ring
//
// Everything above this seam is written in terms of etna_cmd_stream / etna_bo;
// everything below implements those against hardware. By providing this API on
// baremetal (backed by our register code instead of ioctls), Mesa's operation
// emitters (etnaviv_rs.c, etnaviv_clear_blit.c, ...) port near-mechanically.
//
// Naming mirrors libdrm's C API (etna_bo, etna_cmd_stream, ...) so that ported
// Mesa/libdrm code reads the same; we express it as C++ in namespace `etna`.
// Reference: libdrm etnaviv_drmif.h, Mesa src/gallium/drivers/etnaviv/. Both MIT.
//
// -----------------------------------------------------------------------------
//  Status legend (this header doubles as the build roadmap):
//    [HAVE]    logic already exists in gpu/main.cc; to be moved/wrapped here
//    [NEW]     small new glue to write for the first milestone
//    [LATER]   deferred: the WAIT/LINK ring, GPU MMU, fences-per-submit, 3D
// -----------------------------------------------------------------------------
//
//  MEMORY MODEL
//  The GPU masters *physical* DDR directly (its MMU is unused -- the RS engine
//  works in bypass). We run the CPU with a flat identity map, so for any buffer:
//        cpu_pointer == physical_address == gpu_address
//  A "buffer object" (Bo) is therefore just a physically-contiguous DDR
//  allocation. Relocations (below) keep the abstraction so that (a) Mesa code
//  ports unchanged and (b) if a GPU MMU is added [LATER], only the Bo->gpu_addr
//  mapping changes, not the callers.
//
//  CACHE COHERENCY (the sharp edge)
//  The CPU caches DDR; the GPU does not see the CPU's dirty lines, and the CPU
//  does not see GPU writes until it invalidates. Every Bo access is bracketed:
//    - before the GPU reads CPU-written data:  clean  (flush) the range to DDR
//    - after the GPU writes, before CPU reads: invalidate the range
//  These map to clean_dcache_range()/invalidate_dcache_range() (system_reg.hh).
//  The API makes this explicit via Bo::cpu_prep()/cpu_fini() so callers can't
//  forget it (this is the #1 source of "half-written buffer" bugs).

namespace etna
{

// The event id our RS/PE ops assign to "operation complete", raised FROM_PE so
// it latches only after the pipeline drains and caches flush. wait() keys off
// bit `kEventComplete` by default.
inline constexpr uint32_t kEventComplete = 2;

// -----------------------------------------------------------------------------
//  Relocations
// -----------------------------------------------------------------------------
// Access intent for a Bo referenced by a command stream. Drives cache handling
// and (later) fence/dependency tracking. Mirrors ETNA_RELOC_READ/WRITE.
enum RelocFlags : uint32_t {
	RelocRead = 1 << 0,
	RelocWrite = 1 << 1,
};

// A reference to a Bo emitted into a command stream. In Linux the kernel patches
// the address at submit time; here gpu_addr is known up front (identity map), so
// emit_reloc() writes it immediately -- but the struct is kept so callers read
// identically to Mesa and so an MMU can be slotted in later. Mirrors etna_reloc.
struct Bo; // fwd
struct Reloc {
	const Bo *bo;
	uint32_t flags;	 // RelocRead | RelocWrite
	uint32_t offset; // byte offset into the bo
};

// -----------------------------------------------------------------------------
//  Bo -- buffer object (a physically-contiguous DDR allocation)   [NEW]
// -----------------------------------------------------------------------------
// Mirrors etna_bo. On baremetal a Bo is carved from a fixed DDR pool (see
// Gpu::alloc()); there is no refcount/handle/dmabuf machinery.
struct Bo {
	uint32_t phys = 0; // physical == cpu == gpu address (identity map)
	uint32_t bytes = 0;
	bool cacheable = true;

	void *map() const { return reinterpret_cast<void *>(static_cast<uintptr_t>(phys)); }
	uint32_t gpu_addr() const { return phys; } // what a reloc emits
	uint32_t size() const { return bytes; }
	explicit operator bool() const { return phys != 0; }

	// Cache bracketing for CPU access, matching etna_bo_cpu_prep/_fini semantics.
	// op = RelocWrite: CPU is about to WRITE data the GPU will read  -> clean after
	// op = RelocRead : CPU is about to READ data the GPU wrote       -> invalidate
	// [NEW] implemented in etna.cc via clean_/invalidate_dcache_range().
	// const: they touch the backing memory, not the handle.
	void cpu_prep(uint32_t op) const; // wait for pending GPU work (once fences exist) + prep
	void cpu_fini(uint32_t op) const; // finish CPU access: clean if we wrote
};

// -----------------------------------------------------------------------------
//  CmdStream -- a growable command buffer fed to the FE          [HAVE/NEW]
// -----------------------------------------------------------------------------
// Mirrors etna_cmd_stream. Backing store is itself a Bo (the FE DMA-reads it),
// so it is cache-cleaned before every kick. `emit`/`reserve`/`reloc` match the
// libdrm inlines; `set_state`/`set_state_reloc` match the Mesa helpers so that
// ported op code (etnaviv_rs.c) compiles against this unchanged.
class CmdStream {
public:
	// Backed by a Bo of `words` dwords. [NEW] Gpu::new_cmd_stream() creates one.
	CmdStream(Bo backing, uint32_t words)
		: buf_{static_cast<uint32_t *>(backing.map())}
		, cap_{words}
		, bo_{backing}
	{
	}

	uint32_t avail() const { return cap_ - len_; }
	uint32_t offset() const { return len_; } // in dwords
	const Bo &bo() const { return bo_; }

	// Ensure room for n more dwords. [LATER] with a ring, a full stream flushes
	// and wraps; for now it just asserts capacity.
	void reserve(uint32_t n); // [NEW]

	// Append one command dword. Mirrors etna_cmd_stream_emit().
	void emit(uint32_t data) { buf_[len_++] = data; }

	// Emit a Bo's gpu address (+reloc.offset). Records read/write intent for
	// cache/fence handling. Mirrors etna_cmd_stream_reloc(). [NEW]
	void emit_reloc(const Reloc &r);

	// --- Mesa-compatible state helpers (mirror etnaviv_emit.h) -------------
	// LOAD_STATE header + one data word (writes one state register).
	void set_state(uint32_t state_addr, uint32_t value)
	{
		emit(VivanteGpu::cmd_load_state(state_addr));
		emit(value);
	}
	// LOAD_STATE header + a reloc data word (state register = bo address).
	void set_state_reloc(uint32_t state_addr, const Reloc &r)
	{
		emit(VivanteGpu::cmd_load_state(state_addr));
		emit_reloc(r);
	}
	// Pad to a 64-bit (2-dword) boundary with NOP. Commands must be 64-bit
	// aligned; a lone LOAD_STATE needs padding.
	void align()
	{
		if (len_ & 1)
			emit(VivanteGpu::CMD_NOP);
	}

	// --- Synchronization primitives (mirror etnaviv_buffer.c CMD_* helpers) -
	// FE waits for `to` engine to reach a matching semaphore. Used to make the
	// FE block until the PE/RS pipeline drains. (SYNC_RECIPIENT_PE etc.)
	void stall(uint32_t from, uint32_t to); // [HAVE] (stall_fe_on_pe in main.cc)
	// Queue an event: latches HI_INTR_ACKNOWLEDGE bit `id` when `engine`
	// (GL_EVENT_FROM_PE / _FE) processes it. FROM_PE is the true completion
	// signal (pipelined behind writes); FROM_FE fires early -- do not trust it.
	void event(uint32_t id, uint32_t engine)
	{
		set_state(VivanteGpu::GL_EVENT, id | engine);
	}
	// Flush the GPU-internal caches to DDR (color/depth). Needed before the
	// completion event or GPU writes may linger. [HAVE]
	void flush_cache(uint32_t bits = VivanteGpu::GL_FLUSH_CACHE_COLOR | VivanteGpu::GL_FLUSH_CACHE_DEPTH)
	{
		set_state(VivanteGpu::GL_FLUSH_CACHE, bits);
	}

	// Rewind for reuse (single-shot model). [LATER] a ring won't need this.
	void reset() { len_ = 0; }

private:
	uint32_t *buf_;
	uint32_t cap_;
	uint32_t len_ = 0;
	Bo bo_;
};

// -----------------------------------------------------------------------------
//  Fence -- completion token for a submission                    [NEW]
// -----------------------------------------------------------------------------
// Mirrors the (timestamp, pipe_wait) fence model of libdrm. A submission
// completes when its GL_EVENT(FROM_PE) latches HI_INTR_ACKNOWLEDGE bit
// `event_id`. Each submit gets a distinct rolling event id (1..29), so several
// submissions can be in flight and waited for independently.
struct Fence {
	uint32_t event_id = 0; // 0 = null; else the HI_INTR_ACKNOWLEDGE bit to poll
	uint32_t seqno = 0;
	explicit operator bool() const { return event_id != 0; }
};

// -----------------------------------------------------------------------------
//  Gpu -- device + core + pipe, collapsed (we have exactly one)  [HAVE/NEW]
// -----------------------------------------------------------------------------
// libdrm splits this into etna_device / etna_gpu / etna_pipe (for multi-device,
// multi-core, 2D-vs-3D pipes). We have one core and drive it from EL3, so this
// single object owns hardware bring-up, a DDR allocator, and submission.
class Gpu {
public:
	// Chip identity, read from the HI_CHIP_* registers and (authoritatively)
	// the etnaviv hardware database. Mirrors etna_gpu get_param + etna_core_info.
	struct Info {
		uint32_t model, revision, product_id, customer_id;
		uint32_t features, minor_features[6];
		bool has_blt;	   // FALSE on MP25 despite the feature bit (see hwdb)
		bool sec_mode;	   // security-enabled core: FE started via secure bank
		uint32_t pixel_pipes;
	};

	// Full bring-up: power (VDDGPU), clock (PLL3), RIF, reset, identify. Returns
	// false with a diagnostic on any failure. [HAVE] (steps 1-5 of main.cc).
	bool init();
	const Info &info() const { return info_; }

	// Allocate `bytes` of physically-contiguous DDR from the GPU pool, aligned
	// to `align` (default 64 = cache line). Mirrors etna_bo_new. [NEW]
	// The pool is a fixed DDR carve-out; freeing is [LATER] (bump alloc for now).
	Bo alloc(uint32_t bytes, uint32_t align = 64, bool cacheable = true);

	// Create a command stream backed by a freshly allocated Bo. [NEW]
	CmdStream new_cmd_stream(uint32_t words = 1024);

	// Submit a command stream (which must contain an op's work + PE drain, NO
	// END -- the op helpers emit exactly that). Appends it to the persistent
	// WAIT/LINK ring plus a completion trailer (event + wait + link), then
	// diverts the FE by patching its idle WAIT into a LINK to the new block.
	// The FE is never reset per submit -- it has been running the ring since
	// init(). Returns a Fence carrying the op's rolling event id. [NEW: ring]
	Fence submit(CmdStream &cs);

	// Block until `f` completes or timeout. Mirrors etna_pipe_wait. Polls
	// HI_INTR_ACKNOWLEDGE (accumulating, since the read is destructive) for
	// f's event bit; returns false on timeout / AXI / MMU error. Several
	// fences can be outstanding and waited in any order. [HAVE, wrap]
	// [LATER] deliver via the GPU IRQ (GIC SPI 215) for true async.
	bool wait(Fence f, uint32_t timeout_us = 1'000'000);

	// Print FE/PE/IAC/RISAF diagnostics -- the instrumentation from bring-up,
	// for when a submission times out or errors.
	void dump_status(const char *msg);

	// Convenience: submit + wait. Mirrors etna_cmd_stream_finish().
	bool submit_and_wait(CmdStream &cs, uint32_t timeout_us = 1'000'000)
	{
		Fence f = submit(cs);
		return f && wait(f, timeout_us);
	}

private:
	// Persistent WAIT/LINK command ring (non-cacheable DDR, FE-coherent). The
	// FE spins on an idle WAIT/LINK at `ring_tail_`; submit() appends after
	// `ring_head_` and patches the tail's WAIT into a LINK to the new block.
	bool ring_init();

	Info info_{};
	uint32_t seqno_ = 0;	   // monotonic submission counter (diagnostic)
	uint32_t ring_base_ = 0;   // physical address of the ring (== cpu == gpu)
	uint32_t ring_dwords_ = 0; // ring capacity
	uint32_t ring_head_ = 0;   // dword cursor for the next append (qword-aligned)
	uint32_t ring_tail_ = 0;   // dword offset of the WAIT the FE is idling on
	uint32_t next_event_ = 1;  // rolling completion event id (1..29)

	// Completion is delivered by the GPU IRQ (GIC SPI 215): the ISR is the sole
	// reader of HI_INTR_ACKNOWLEDGE (a read clears it and de-asserts the line)
	// and ORs the fired event bits into intr_acc_; wait() consumes its bit and
	// sleeps on WFE in between. Atomic since ISR and wait() touch it concurrently.
	std::atomic<uint32_t> intr_acc_{0};
	void on_irq(); // the GPU interrupt handler
};

// =============================================================================
//  2D operations -- built on the seam above                     [HAVE, refactor]
// =============================================================================
// These are the ported Mesa RS emitters (etna_clear_blit_rs / etna_submit_rs_state
// in etnaviv_rs.c), re-expressed against CmdStream. Each *emits* into a stream;
// the caller submits. This is the "usable API" the tests refactor onto.
//
// The MP25 core has no 2D pipe and no BLT engine, so clears/blits go through the
// RS ("resolve") engine. 3D (alpha blend, rotate, arbitrary geometry) needs the
// programmable pipe + offline-compiled shaders and is a separate header [LATER].

enum class Format : uint32_t {
	A8R8G8B8 = VivanteGpu::RS_FORMAT_A8R8G8B8, // 32bpp; more added as needed
};

enum BlitFlags : uint32_t {
	BlitNone = 0,
	BlitSwapRB = 1 << 0, // RGBA<->BGRA (VIVS_RS_CONFIG_SWAP_RB)
	BlitFlipY = 1 << 1,	 // vertical flip (VIVS_RS_CONFIG_FLIP)
};

// =============================================================================
//  Compute -- the programmable shader cores (PPU / unified shader)
// =============================================================================
// The RS engine above is fixed-function (fill/blit only). For arbitrary per-pixel
// math we run a *shader* on the PPU (parallel processing unit = unified shader
// cores), dispatched by the same FE command ring. Implemented in etna_compute.cc;
// the shader assembler + kernel library is ppu_asm.hh.
//
//  MODEL
//   - A *kernel* is a short shader program that runs once per pixel across the
//     whole image, in parallel. Build one with a ppu::build_*_shader.
//   - It reads 1..3 input images and writes 1 output image, all the same
//     width x height u8 buffer. Images bind to descriptor *uniforms*:
//     input0 -> c0, output -> c1, input1 -> c2, input2 -> c3.
//   - "u8 image" is just bytes: an ARGB8888 W x H framebuffer is a u8 image of
//     (4*W) x H, and a per-byte kernel then operates independently per channel.
//   - `width` is in BYTES and must be a multiple of 16 (the EVIS vector width).
//
//  THE OP TOOLKIT (all hardware-verified per-byte u8 SIMD; see ppu_asm.hh)
//     ADD, IADDSAT (saturating), IMULLO (low half), IMULHI/mul_hi (high half),
//     AND, NOT, u32 immediates, and img_load/img_store. New kernels compose these
//     with the emit_* helpers in ppu_asm.hh.
//
//  USAGE
//     Kernel add   = make_kernel(gpu, ppu::build_add_shader);       // compile once
//     Kernel blend = make_kernel(gpu, ppu::build_blend_lerp_shader);
//     compute(gpu, add,   out, in,        W, H);   // out = in + in
//     compute(gpu, blend, out, a, b, alpha, W, H); // per-pixel alpha lerp
//     out.cpu_prep(RelocRead);                     // invalidate before CPU reads

// A compiled kernel: the shader binary in GPU memory + its instruction and
// register counts (which feed the dispatch's InstCount/RegCount). Build once
// with make_kernel(), then reuse across many compute() calls.
struct Kernel {
	Bo binary;				  // the shader program in GPU memory
	uint32_t inst_dwords = 0; // program size in dwords (4 per instruction)
	uint32_t reg_count = 0;	  // temp registers the program uses
	explicit operator bool() const { return bool(binary); }
};

// A ppu::build_*_shader function: writes the program into inst[] (up to 8
// instructions = 32 dwords) and returns its ShaderInfo.
using ShaderBuilder = ppu::ShaderInfo (*)(uint32_t *, uint32_t, uint32_t);

// Compile a shader builder into a reusable Kernel (allocates GPU memory for the
// program and uploads it). Returns an empty Kernel (operator bool == false) on
// allocation failure.
Kernel make_kernel(Gpu &gpu, ShaderBuilder build);

// Run `k` over its input image(s) -> `out` (all width x height u8, width a
// multiple of 16). Blocks until the GPU finishes. The three overloads bind 1, 2,
// or 3 inputs; use the arity that matches the kernel. The caller handles cache
// bracketing: cpu_fini(RelocWrite) the inputs before, cpu_prep(RelocRead) the
// output after.
bool compute(Gpu &gpu, const Kernel &k, const Bo &out, const Bo &in0, uint32_t width, uint32_t height);
bool compute(Gpu &gpu, const Kernel &k, const Bo &out, const Bo &in0, const Bo &in1, uint32_t width,
			 uint32_t height);
bool compute(Gpu &gpu, const Kernel &k, const Bo &out, const Bo &in0, const Bo &in1, const Bo &in2, uint32_t width,
			 uint32_t height);

// Demo/self-test: builds and runs the whole kernel suite (copy, add, blends,
// multiply, alpha lerp, ...) over gradient data and verifies each on the CPU.
bool compute_test(Gpu &gpu);

// 3D graphics pipe (etna_3d.cc): draw one screen-covering solid-color triangle
// through the programmable pipeline (vertex fetch -> VS -> rasterizer -> FS ->
// pixel engine) and verify the render target changed from clear -> triangle
// color. First light for the graphics path.
bool triangle_test(Gpu &gpu);
bool triangle_color_test(Gpu &gpu);

// Solid-color fill of `dst` (width x height, linear). Emits the RS clear
// sequence + PE drain (stall + cache flush + stall); submit() adds the ring
// completion trailer. Note: no END -- END would halt the FE's ring loop.
void clear(CmdStream &cs, const Bo &dst, uint32_t width, uint32_t height, uint32_t argb);

// Copy `src` -> `dst` (same size, linear) with optional per-pixel transform
// (R<->B swap, flip), including the completion trailer. Replaces
// test_rs_blit_convert. [HAVE -> move here]
void blit(CmdStream &cs,
		  const Bo &dst,
		  const Bo &src,
		  uint32_t width,
		  uint32_t height,
		  Format fmt = Format::A8R8G8B8,
		  uint32_t flags = BlitNone);

// =============================================================================
//  Usage sketch -- how the current tests become API calls
// =============================================================================
//
//   etna::Gpu gpu;
//   if (!gpu.init()) return;                         // steps 1-5
//
//   auto fb  = gpu.alloc(W * H * 4);                 // dest framebuffer
//   auto src = gpu.alloc(W * H * 4);                 // source image
//   auto cs  = gpu.new_cmd_stream();
//
//   // --- fill (step 7) ---
//   etna::clear(cs, fb, W, H, 0x4D5A11AC);
//   gpu.submit_and_wait(cs);
//   fb.cpu_prep(etna::RelocRead);                    // invalidate before CPU reads
//   // ... verify fb ...
//
//   // --- blit + R<->B convert (step 8) ---
//   /* fill src on the CPU */ src.cpu_fini(etna::RelocWrite);   // clean to DDR
//   cs.reset();
//   etna::blit(cs, fb, src, W, H, etna::Format::A8R8G8B8, etna::BlitSwapRB);
//   gpu.submit_and_wait(cs);
//   fb.cpu_prep(etna::RelocRead);
//   // ... verify fb == swap_rb(src) ...
//
// Once the WAIT/LINK ring lands [LATER], multiple ops queue back-to-back without
// per-submit reset, and submit()/wait() become truly async (build the next
// stream while the GPU drains the current one).

} // namespace etna
