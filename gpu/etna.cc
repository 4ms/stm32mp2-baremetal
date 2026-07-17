#include "etna.hh"
#include "aarch64/system_reg.hh" // cache ops, read_cntpct/read_cntfreq
#include "drivers/hal_cnt.hh"	 // udelay
#include "drivers/rcc.hh"
#include "drivers/rcc_pll.hh"
#include "drivers/rcc_xbar.hh"
#include "gpu_io.hh"			  // gpu_read/write, wait_idle
#include "interrupt/interrupt.hh" // InterruptManager (GPU IRQ)
#include "print/print.hh"
#include "stm32mp2xx.h"

// PMIC buck3 fallback (pmic.cc) -- only used if TF-A did not enable VDDGPU.
bool enable_buck3_on_pmic();

namespace etna
{
using namespace VivanteGpu;

// =============================================================================
//  DDR pool + hardware bring-up (file-local; moved from the example's main.cc)
// =============================================================================
namespace
{

// GPU buffers live in a fixed DDR region starting at 0x9000'0000 (normal, secure, cacheable)
constexpr uint32_t PoolBase = 0x90000000;
constexpr uint32_t PoolSize = 64 * 1024 * 1024;
uint32_t pool_next = PoolBase;

// Even with buck3 up, the GPU domain is electrically isolated until software
// confirms the supply with the PWR voltage monitor and sets "supply valid".
// The monitor must STAY enabled: its live output releases the GPU power-domain
// reset (vgpu_rstn), which also holds PLL3's registers in reset if dropped.
bool power_up_vddgpu(unsigned timeout_us)
{
	PWR->CR12 |= PWR_CR12_GPUVMEN;
	while (!(PWR->CR12 & PWR_CR12_VDDGPURDY)) {
		if (--timeout_us == 0) {
			PWR->CR12 &= ~PWR_CR12_GPUVMEN;
			print("etna: VDDGPU not present (CR12 = 0x", Hex{PWR->CR12}, ")\n");
			return false;
		}
		udelay(1);
	}
	PWR->CR12 |= PWR_CR12_GPUSV;
	return true;
}

// PLL3 physically lives inside the GPU subsystem, so the GPU must be powered
// and clocked before PLL3 can be programmed. GPURST is pulsed last, once the
// kernel clock is running (RM0457 requires fref < ck_ker_gpu during reset).
bool clock_and_reset_gpu()
{
	RCC_Enable::GPU_::set();
	udelay(10);

	// 800 MHz needs the 0.90V VDDGPU; set postdiv1 to 4 for 400 MHz at 0.80 V.
	constexpr RCC_Clocks::PLLSettings pll3{
		.src = RCC_Clocks::MuxSelSource::hse,
		.mult = 40,
		.refdiv = 1,
		.postdiv1 = 2, // use 4 for 400MHz
		.postdiv2 = 1,
		.frac = 0,
	};
	static_assert(pll3.calc_freq(40'000'000) == 800'000'000);
	RCC_Clocks::set_pll<RCC_Clocks::PLL3>(pll3);

	unsigned timeout = 10'000;
	while (!RCC_Clocks::PLL3::Ready::read()) {
		if (--timeout == 0) {
			print("etna: PLL3 did not lock (PLL3CFGR1 = 0x", Hex{RCC->PLL3CFGR1}, ")\n");
			return false;
		}
		udelay(1);
	}

	RCC_Reset::GPU_::set();
	udelay(10);
	RCC_Reset::GPU_::clear();
	timeout = 1000;
	while (RCC_Reset::GPU_::read()) {
		if (--timeout == 0) {
			print("etna: GPU reset never released (GPUCFGR = 0x", Hex{RCC->GPUCFGR}, ")\n");
			return false;
		}
		udelay(1);
	}
	return true;
}

// The GPU has two clocks: the core/shader clock (ck_ker_gpu = PLL3, 800 MHz)
// and the AXI/memory-interface clock (CK_BUS_GPU = ck_icn_m_gpu = flexgen channel 59). 
// RM says 600MHz is typical value for ck_icn_m_gpu
uint32_t boost_gpu_memclock(uint32_t target_hz)
{
	using namespace RCC_Clocks;
	auto s = get_pll_settings<PLL4>();
	uint32_t ref = s.src == MuxSelSource::hse ? 40'000'000u : s.src == MuxSelSource::hsi ? 64'000'000u : 0u;
	uint32_t pll4 = ref ? s.calc_freq(ref) : 0;
	if (pll4 < 100'000'000u || pll4 > 3'000'000'000u) {
		print("etna: PLL4 unusable as GPU mem-clock source (", pll4 / 1'000'000, " MHz); leaving flexgen59 on HSI\n");
		return 0;
	}

	uint32_t ratio = (pll4 + target_hz / 2) / target_hz; // round to nearest
	if (ratio < 1)
		ratio = 1;
	if (ratio > 64)
		ratio = 64;

	FlexbarConf{.PLL = FlexbarConf::PLLx::_4, .findiv = static_cast<uint8_t>(ratio - 1), .prediv = 0}.init(59);

	uint32_t achieved = pll4 / ratio;
	print("etna: GPU mem-clock ~", achieved / 1'000'000, " MHz\n");
	return achieved;
}

// The GPU's CID and security both are inherited from RISUP79 (RM0457 sec 8.3)
// Set peripheral 79 secure because we need to access secure memory
// (or else RISAF4 will block us, and we'll get IAC faults).
// RIMC adds the privileged bit, but it seems to work without that.
void setup_rif()
{
	RISC->SECCFGR[2] |= (1 << (79 - 64));
	auto attr = RIMC->ATTR[9]; // RIF_MCID_GPU = 9
	RIMC->ATTR[9] = (attr & ~RIMC_ATTR_CIDSEL) | RIMC_ATTR_MSEC | RIMC_ATTR_MPRIV;
}

// Soft-reset the core (mirrors etnaviv_hw_reset() in linux/drivers/gpu/drm/etnaviv/etnaviv_gpu.c).
bool reset_gpu_core()
{
	for (unsigned tries = 0; tries < 10; tries++) {
		auto control = gpu_read(HI_CLOCK_CONTROL);

		control |= CLK_ISOLATE_GPU;
		gpu_write(HI_CLOCK_CONTROL, control);

		gpu_write(MMUv2_AHB_CONTROL, MMUv2_AHB_CONTROL_RESET);
		gpu_write(HI_CLOCK_CONTROL, control | CLK_SOFT_RESET);
		udelay(20);
		gpu_write(HI_CLOCK_CONTROL, control);
		control &= ~CLK_ISOLATE_GPU;
		gpu_write(HI_CLOCK_CONTROL, control);

		if (!(gpu_read(HI_IDLE_STATE) & IDLE_FE))
			continue;
		if (!(gpu_read(HI_CLOCK_CONTROL) & CLK_IDLE_3D))
			continue;

		gpu_write(MMUv2_AHB_CONTROL, MMUv2_AHB_CONTROL_NONSEC_ACCESS);

		// Set clock speed to empirically determined max speed (0x0)
		control &= ~0x1FC;
		control |= CLK_FSCALE_VAL(0x0);
		gpu_write(HI_CLOCK_CONTROL, control | CLK_FSCALE_CMD_LOAD);
		udelay(1);
		gpu_write(HI_CLOCK_CONTROL, control);

		return true;
	}

	print("etna: GPU did not go idle after soft reset (idle 0x", Hex{gpu_read(HI_IDLE_STATE)}, ")\n");
	return false;
}

// Arm the FE at a command buffer. On this security-enabled core the FE only
// actually starts when the enable is also written through the secure bank.
void arm_fe(uint32_t addr, uint32_t num_dwords)
{
	gpu_write(FE_COMMAND_ADDRESS, addr);
	gpu_write(FE_COMMAND_CONTROL, FE_CONTROL_ENABLE | (num_dwords / 2));
	gpu_write(MMUv2_SEC_COMMAND_CONTROL, FE_CONTROL_ENABLE | (num_dwords / 2));
}

// Append the generic PE-pipeline completion trailer: drain the FE onto the PE,
// flush the color/depth caches to DDR, drain again, latch the completion event,
// then END. Used by every RS/PE op so completion means "results are in DDR".
// Drain the RS/PE pipeline and flush its writes to DDR: stall the FE onto the
// PE, flush the color/depth caches, stall again. The op leaves the pipeline
// drained; submit() appends the ring completion trailer (event + wait + link).
// Deliberately NO event and NO END here -- an END would halt the FE's ring.
void emit_pe_drain(CmdStream &cs)
{
	cs.stall(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
	cs.flush_cache();
	cs.stall(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
}

} // namespace

// =============================================================================
//  Bo
// =============================================================================
void Bo::cpu_prep(uint32_t op) const
{
	// About to read data the GPU wrote: drop stale cache lines first.
	if ((op & RelocRead) && cacheable)
		invalidate_dcache_range(map(), bytes);
}

void Bo::cpu_fini(uint32_t op) const
{
	// Finished writing data the GPU will read: push it out to DDR.
	if ((op & RelocWrite) && cacheable)
		clean_dcache_range(map(), bytes);
}

// =============================================================================
//  CmdStream
// =============================================================================
void CmdStream::reserve(uint32_t n)
{
	// Single-shot model: no ring wrap yet, so this only guards against overflow.
	if (len_ + n > buf_.size())
		print("etna: CmdStream overflow (need ", int(len_ + n), " of ", int(buf_.size()), " dwords)\n");
}

void CmdStream::emit_reloc(const Reloc &r)
{
	// Identity map: the GPU address is just the buffer's physical address.
	// (Access intent r.flags is recorded conceptually; automatic cache/fence
	// tracking off the reloc list is a later enhancement.)
	emit(r.bo->gpu_addr() + r.offset);
}

void CmdStream::stall(uint32_t from, uint32_t to)
{
	set_state(GL_SEMAPHORE_TOKEN, sync_token(from, to));
	emit(CMD_STALL);
	emit(sync_token(from, to));
}

// =============================================================================
//  Gpu
// =============================================================================
bool Gpu::init()
{
	print("etna: bringing up GPU\n");

	if (!power_up_vddgpu(10'000)) {
		print("etna: VDDGPU off -- trying to enable buck3 over I2C7...\n");
		if (!enable_buck3_on_pmic() || !power_up_vddgpu(100'000))
			return false;
	}
	if (!clock_and_reset_gpu())
		return false;
	boost_gpu_memclock(600'000'000); // lift the AXI/memory clock off HSI 64 MHz
	setup_rif();
	if (!reset_gpu_core())
		return false;

	print("\n");

	info_.model = gpu_read(HI_CHIP_MODEL);
	info_.revision = gpu_read(HI_CHIP_REV);
	info_.product_id = gpu_read(HI_CHIP_PRODUCT_ID);
	info_.customer_id = gpu_read(HI_CHIP_CUSTOMER_ID);
	info_.features = gpu_read(HI_CHIP_FEATURE);
	info_.minor_features[0] = gpu_read(HI_CHIP_MINOR_FEATURE_0);
	info_.minor_features[1] = gpu_read(HI_CHIP_MINOR_FEATURE_1);
	info_.minor_features[2] = gpu_read(HI_CHIP_MINOR_FEATURE_2);
	info_.minor_features[3] = gpu_read(HI_CHIP_MINOR_FEATURE_3);
	info_.minor_features[4] = gpu_read(HI_CHIP_MINOR_FEATURE_4);
	info_.minor_features[5] = gpu_read(HI_CHIP_MINOR_FEATURE_5);
	// mainline linux etnaviv_hwdb.c would match our model/revision/customer_id and report
	// these fields (without even reading minor_features from the chip):
	// .model = 0x8000,
	// .revision = 0x6205,
	// .product_id = 0x80003,
	// .customer_id = 0x15,
	// .eco_id = 0,
	// .stream_count = 16,
	// .register_max = 64,
	// .thread_count = 512,
	// .shader_core_count = 2,
	// .nn_core_count = 2,
	// .vertex_cache_size = 16,
	// .vertex_output_buffer_size = 1024,
	// .pixel_pipes = 1,
	// .instruction_count = 512,
	// .num_constants = 320,
	// .buffer_size = 0,
	// .varyings_count = 16,
	// .features = 0xe0287c8d,
	// .minor_features0 = 0xc1799eff,
	// .minor_features1 = 0xfefbfad9,
	// .minor_features2 = 0xeb9d4fbf,
	// .minor_features3 = 0xedfffced,
	// .minor_features4 = 0xdb0dafc7,
	// .minor_features5 = 0x7b5ac333,
	// .minor_features6 = 0xfcce6000,
	// .minor_features7 = 0x03fbfa6f,
	// .minor_features8 = 0x00ef0ef0,
	// .minor_features9 = 0x0eca703c,
	// .minor_features10 = 0x898048f0,
	// .minor_features11 = 0x00000034,
	//
	// The feature registers falsely advertise BLT on this core (minor features 5, bit 31)
	// So, fills/blits use the RS engine
	info_.has_blt = false;
	info_.sec_mode = true;
	info_.pixel_pipes = 1;

	if (info_.model == 0 || info_.model == 0xFFFFFFFF) {
		print("etna: GPU not responding (model reads 0x", Hex{info_.model}, ")\n");
		return false;
	}
	print("etna: GC model 0x", Hex{info_.model}, " rev 0x", Hex{info_.revision});
	print(" (product 0x", Hex{info_.product_id}, ", customer 0x", Hex{info_.customer_id}, ")\n");

	// Start the persistent command ring: this is the only time the FE is armed.
	// From here it spins on the ring's WAIT/LINK idle loop, and submit() feeds
	// it work by patching -- no more per-op resets.
	return ring_init();
}

// The ring lives in NON-CACHEABLE DDR so CPU writes are immediately visible to
// the FE's DMA (a dsb is enough; no cache maintenance on the hot path). We use
// the shared "noncache" block the MMU maps at 0x8A000000 (see shared/mmu/mmu.cc).
bool Gpu::ring_init()
{
	ring_base_ = 0x8A000000;
	ring_dwords_ = 4096; // 16 KB
	auto ring = reinterpret_cast<volatile uint32_t *>(static_cast<uintptr_t>(ring_base_));

	// Home idle loop at offset 0: WAIT; LINK -> WAIT. (WAIT is 1 dword but
	// occupies a qword; the FE resumes at the next qword, the LINK.)
	ring[0] = cmd_wait(200);
	ring[1] = 0; // pad
	ring[2] = cmd_link(2);
	ring[3] = ring_base_ + 0; // link back to the WAIT at offset 0
	dsb_sy();

	ring_tail_ = 0; // the WAIT the FE idles on
	ring_head_ = 4; // next free dword (qword-aligned)
	next_event_ = 1;
	intr_acc_.store(0);

	// AXI cache attributes: AWCACHE(2)|ARCACHE(2) = "modifiable/bufferable" --
	// permits the NIC/DDR controller to merge the GPU's accesses into efficient
	// bursts. etnaviv_gpu_hw_init() writes this on every boot ("cacheable, no
	// allocate")
	// TODO: default is 0x222200, which is what we write. Can we just go with the default?
	auto old_val = gpu_read(HI_AXI_CONFIG);
	gpu_write(HI_AXI_CONFIG, (2u << 20) | (2u << 16) | (2u << 8) | (2u << 12));
	print("etna: HI_AXI_CONFIG reset value 0x", Hex{old_val});
	print(" -> 0x", Hex{gpu_read(HI_AXI_CONFIG)}, " (expect 0x22200)\n");

	// Unmask interrupt sources so GL_EVENTs latch in HI_INTR_ACKNOWLEDGE, and
	// take the GPU interrupt (GIC SPI 215) rather than polling. The ISR is the
	// ONLY reader of HI_INTR_ACKNOWLEDGE.
	gpu_write(HI_INTR_ENBL, 0xFFFFFFFF);
	gpu_read(HI_INTR_ACKNOWLEDGE); // clear stale bits
	InterruptManager::register_and_start_isr(GPU_IRQn, 0, 0, [this] { on_irq(); });

	// Arm the FE once, on the ring home (prefetch = 2 qwords = the WAIT/LINK).
	arm_fe(ring_base_, 2);
	return true;
}

// GPU interrupt handler. Reading HI_INTR_ACKNOWLEDGE returns the fired event
// bits AND clears them / de-asserts the GIC line, so we read it exactly once
// and accumulate. SEV wakes any waiter parked on WFE (belt-and-suspenders for
// the case where the IRQ landed just before the waiter's WFE).
void Gpu::on_irq()
{
	uint32_t ack = gpu_read(HI_INTR_ACKNOWLEDGE);
	if (ack)
		intr_acc_.fetch_or(ack, std::memory_order_release);
	asm volatile("sev" ::: "memory");
}

Bo Gpu::alloc(uint32_t bytes, uint32_t align, bool cacheable)
{
	uint32_t base = (pool_next + (align - 1)) & ~(align - 1);
	uint32_t next = base + ((bytes + 63) & ~63u); // round size up to a cache line
	if (next > PoolBase + PoolSize) {
		print("etna: GPU pool exhausted (need ", int(bytes), " bytes)\n");
		return Bo{}; // null
	}
	pool_next = next;
	return Bo{.phys = base, .bytes = bytes, .cacheable = cacheable};
}

CmdStream Gpu::new_cmd_stream(uint32_t words)
{
	Bo bo = alloc(words * 4, 64);
	return CmdStream{bo, words};
}

Fence Gpu::submit(CmdStream &cs)
{
	auto ring = reinterpret_cast<volatile uint32_t *>(static_cast<uintptr_t>(ring_base_));

	// The block we append: the op's work (from cs) + a completion trailer of
	// EVENT (2 dwords) + WAIT (2) + LINK (2) = 6 dwords.
	uint32_t op_dw = cs.offset();
	uint32_t block_dw = op_dw + 6;

	// Wrap if it won't fit. Safe only because callers wait for completion (the
	// FE is parked at the current tail, far from offset 0) before reusing the
	// ring start. A real free-cursor would let us wrap while work is queued.
	if (ring_head_ + block_dw > ring_dwords_)
		ring_head_ = 4; // keep the home wait-link at 0..3 intact

	uint32_t start = ring_head_;
	uint32_t w = start;

	// Copy the op's commands into the ring (cs backing is cacheable and
	// CPU-written, so a plain read is coherent; the ring is non-cacheable).
	auto src = cs.bo().span<const uint32_t>();
	for (uint32_t i = 0; i < op_dw; i++)
		ring[w++] = src[i];

	// Completion trailer: latch a rolling event id FROM_PE, then a fresh idle
	// WAIT/LINK that becomes the new tail.
	uint32_t event_id = next_event_;
	next_event_ = (next_event_ % 29) + 1;	// 1..29 (avoid bits 30/31 = MMU/AXI error)
	intr_acc_.fetch_and(~(1u << event_id)); // drop any stale accumulated bit for this id
	ring[w++] = cmd_load_state(GL_EVENT);
	ring[w++] = event_id | GL_EVENT_FROM_PE;

	uint32_t new_wait = w;
	ring[w++] = cmd_wait(200);
	ring[w++] = 0; // pad
	ring[w++] = cmd_link(2);
	ring[w++] = ring_base_ + new_wait * 4; // link back to this WAIT (idle)
	dsb_sy();							   // the whole block is in DDR before we divert the FE

	// Divert the FE: replace the tail's WAIT with a LINK to `start`. Write the
	// target first, barrier, then the LINK header -- so the FE (which may fetch
	// at any instant) never sees a LINK header pointing at a stale address.
	// (Mirrors etnaviv_buffer_replace_wait.)
	uint32_t prefetch = block_dw / 2; // qwords
	ring[ring_tail_ + 1] = ring_base_ + start * 4;
	dsb_sy();
	ring[ring_tail_ + 0] = cmd_link(prefetch);
	dsb_sy();

	ring_tail_ = new_wait;
	ring_head_ = w;
	return Fence{.event_id = event_id, .seqno = ++seqno_};
}

bool Gpu::wait(Fence f, uint32_t timeout_us)
{
	if (!f)
		return false;

	// Block until the ISR records f's event bit in intr_acc_. We do NOT read
	// HI_INTR_ACKNOWLEDGE here -- that would steal bits from the ISR. Sleep on
	// WFE between checks so the CPU (and the GPU register bus) is idle while
	// the GPU works; the ISR's SEV/IRQ wakes us.
	// Caveat: the deadline is only evaluated on wake, so a totally hung GPU
	// (no interrupt at all) would block. GPU *errors* do interrupt (bits
	// 30/31), so those are caught. A timer-backstop wake would close the gap.
	const uint32_t done_bit = 1u << f.event_id;
	const uint32_t err_bits = INTR_AXI_BUS_ERROR | INTR_MMU_EXCEPTION;
	uint64_t deadline = read_cntpct() + (uint64_t)timeout_us * (read_cntfreq() / 1'000'000);

	while (true) {
		uint32_t acc = intr_acc_.load(std::memory_order_acquire);
		if (acc & err_bits) {
			print("etna: GPU error interrupt 0x", Hex{acc}, "\n");
			dump_status("  on submit");
			return false;
		}
		if (acc & done_bit) {
			intr_acc_.fetch_and(~done_bit); // consume it
			return true;
		}
		if (read_cntpct() > deadline) {
			print("etna: submission timed out (event ", int(f.event_id), ")\n");
			dump_status("  timeout");
			return false;
		}
		asm volatile("wfe" ::: "memory"); // woken by the GPU IRQ / the ISR's SEV
	}
}

void Gpu::dump_status(const char *msg)
{
	print(msg);
	print(": FE DMA addr 0x", Hex{gpu_read(FE_DMA_ADDRESS)});
	print(" debug 0x", Hex{gpu_read(FE_DMA_DEBUG_STATE)});
	print(" idle 0x", Hex{gpu_read(HI_IDLE_STATE)});
	print(" axi 0x", Hex{gpu_read(HI_AXI_STATUS)}, "\n");
	print("  GPU MMU sec status 0x", Hex{gpu_read(MMUv2_SEC_STATUS)});
	print(" RISAF4 IASR 0x", Hex{RISAF4->IASR}, "\n");
	auto iac_isr = reinterpret_cast<volatile uint32_t *>(IAC_BASE + 0x80);
	auto iac_icr = reinterpret_cast<volatile uint32_t *>(IAC_BASE + 0x100);
	print("  IAC violations:");
	for (unsigned i = 0; i < 6; i++) {
		print(" 0x", Hex{iac_isr[i]});
		iac_icr[i] = 0xFFFFFFFF;
	}
	print("\n");
}

// =============================================================================
//  2D operations (RS engine) -- ports of Mesa etnaviv_rs.c onto CmdStream
// =============================================================================
void clear(CmdStream &cs, const Bo &dst, uint32_t width, uint32_t height, uint32_t argb)
{
	cs.reserve(64);
	// A8R8G8B8, linear. CLEAR_CONTROL enables the fill; FILL_VALUE x4 is the color.
	cs.set_state(RS_CONFIG, RS_FORMAT_A8R8G8B8 | (RS_FORMAT_A8R8G8B8 << 8));
	cs.set_state(RS_SOURCE_STRIDE, 0);
	cs.set_state(RS_DEST_STRIDE, width * 4);
	cs.set_state_reloc(RS_PIPE_SOURCE_ADDR0, {&dst, RelocRead, 0}); // unused, emitted like Mesa
	cs.set_state_reloc(RS_PIPE_DEST_ADDR0, {&dst, RelocWrite, 0});
	cs.set_state(RS_PIPE_OFFSET0, 0);
	cs.set_state(RS_PIPE_OFFSET1, 0);
	cs.set_state(RS_WINDOW_SIZE, width | (height << 16));
	cs.set_state(RS_DITHER0, 0xFFFFFFFF);
	cs.set_state(RS_DITHER1, 0xFFFFFFFF);
	cs.set_state(RS_CLEAR_CONTROL, RS_CLEAR_CONTROL_ENABLED1 | 0xFFFF);
	cs.set_state(RS_FILL_VALUE0 + 0x0, argb);
	cs.set_state(RS_FILL_VALUE0 + 0x4, argb);
	cs.set_state(RS_FILL_VALUE0 + 0x8, argb);
	cs.set_state(RS_FILL_VALUE0 + 0xC, argb);
	cs.set_state(RS_EXTRA_CONFIG, 0);
	cs.set_state(RS_SINGLE_BUFFER, 1);
	cs.set_state(RS_KICKER, RS_KICK);
	cs.set_state(RS_SINGLE_BUFFER, 0);
	emit_pe_drain(cs);
}

void blit(CmdStream &cs, const Bo &dst, const Bo &src, uint32_t width, uint32_t height, Format fmt, uint32_t flags)
{
	cs.reserve(64);
	uint32_t config = static_cast<uint32_t>(fmt) | (static_cast<uint32_t>(fmt) << 8);
	if (flags & BlitSwapRB)
		config |= RS_CONFIG_SWAP_RB;
	if (flags & BlitFlipY)
		config |= RS_CONFIG_FLIP;

	cs.set_state(RS_CONFIG, config);
	cs.set_state(RS_SOURCE_STRIDE, width * 4);
	cs.set_state(RS_DEST_STRIDE, width * 4);
	cs.set_state_reloc(RS_PIPE_SOURCE_ADDR0, {&src, RelocRead, 0});
	cs.set_state_reloc(RS_PIPE_DEST_ADDR0, {&dst, RelocWrite, 0});
	cs.set_state(RS_PIPE_OFFSET0, 0);
	cs.set_state(RS_PIPE_OFFSET1, 0);
	cs.set_state(RS_WINDOW_SIZE, width | (height << 16));
	cs.set_state(RS_DITHER0, 0xFFFFFFFF);
	cs.set_state(RS_DITHER1, 0xFFFFFFFF);
	cs.set_state(RS_CLEAR_CONTROL, 0); // not a clear: real copy
	cs.set_state(RS_EXTRA_CONFIG, 0);
	cs.set_state(RS_SINGLE_BUFFER, 1);
	cs.set_state(RS_KICKER, RS_KICK);
	cs.set_state(RS_SINGLE_BUFFER, 0);
	emit_pe_drain(cs);
}

// Resolve = untile: copy a (basic-)tiled surface -- what the PE renders -- to a
// LINEAR destination a CPU/display can address as y*stride + x*4. This is the
// RS engine's namesake job. Same recipe as blit() with two differences (mirrors
// Mesa etnaviv_rs.c): RS_CONFIG gains SOURCE_TILED, and the source stride
// register holds the tiled stride << 2 (one RS "source row" = a row of 4x4
// tiles = 4 pixel rows). Width must be a multiple of 16 (RS alignment).
void resolve(CmdStream &cs,
			 const Bo &dst,
			 const Bo &src,
			 uint32_t width,
			 uint32_t height,
			 uint32_t src_tiled_stride,
			 uint32_t dst_stride)
{
	cs.reserve(64);
	uint32_t config = RS_FORMAT_A8R8G8B8 | RS_CONFIG_SOURCE_TILED | (RS_FORMAT_A8R8G8B8 << 8);
	cs.set_state(RS_CONFIG, config);
	cs.set_state(RS_SOURCE_STRIDE, src_tiled_stride << 2);
	cs.set_state(RS_DEST_STRIDE, dst_stride);
	cs.set_state_reloc(RS_PIPE_SOURCE_ADDR0, {&src, RelocRead, 0});
	cs.set_state_reloc(RS_PIPE_DEST_ADDR0, {&dst, RelocWrite, 0});
	cs.set_state(RS_PIPE_OFFSET0, 0);
	cs.set_state(RS_PIPE_OFFSET1, 0);
	cs.set_state(RS_WINDOW_SIZE, width | (height << 16));
	cs.set_state(RS_DITHER0, 0xFFFFFFFF);
	cs.set_state(RS_DITHER1, 0xFFFFFFFF);
	cs.set_state(RS_CLEAR_CONTROL, 0);
	cs.set_state(RS_EXTRA_CONFIG, 0);
	cs.set_state(RS_SINGLE_BUFFER, 1);
	cs.set_state(RS_KICKER, RS_KICK);
	cs.set_state(RS_SINGLE_BUFFER, 0);
	emit_pe_drain(cs);
}

} // namespace etna
