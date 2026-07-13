#include "aarch64/system_reg.hh"
#include "drivers/hal_cnt.hh"
#include "drivers/rcc.hh"
#include "drivers/rcc_pll.hh"
#include "gpu_io.hh"
#include "gpu_regs.hh"
#include "interrupt/interrupt.hh"
#include "print/print.hh"
#include <array>
#include <atomic>
#include <cstdint>

// Optional, not working: in pmic.cc:
bool enable_buck3_on_pmic();

// Optional, seems not needed? in gpu_mmuv2.cc:
// bool gpu_mmu_enable();

namespace
{

// Buffers (shared memory between CPU and GPU)
alignas(64) std::array<uint32_t, 128> cmdbuf{};
constexpr uint32_t ImgWidth = 1024;
constexpr uint32_t ImgHeight = 1024;
alignas(64) std::array<uint32_t, ImgWidth * ImgHeight> image;

constexpr uint32_t ClearColor = 0x4D5A11AC;

} // namespace

// Even with buck3 up, the GPU domain is electrically isolated until software
// confirms the supply with the PWR voltage monitor and sets the "supply valid" bit
static bool power_up_vddgpu(unsigned timeout_us)
{
	PWR->CR12 |= PWR_CR12_GPUVMEN;
	while (!(PWR->CR12 & PWR_CR12_VDDGPURDY)) {
		if (--timeout_us == 0) {
			PWR->CR12 &= ~PWR_CR12_GPUVMEN;
			print("VDDGPU is not present (CR12 = 0x", Hex{PWR->CR12}, ")\n");
			return false;
		}
		udelay(1);
	}
	// The monitor must stay enabled:
	PWR->CR12 |= PWR_CR12_GPUSV;
	return true;
}

static bool clock_and_reset_gpu()
{
	// GPU must be powered and have clocks enabled before PLL3 can be setup
	RCC_Enable::GPU_::set();
	udelay(10);
	print("GPU clock enabled\n");

	// 40 MHz HSE / 1 * 30 = 1200 MHz VCO, / 3 = 400 MHz.
	// 400 MHz is a safe speed for the 0.80 V VDDGPU the PMIC provides.
	constexpr RCC_Clocks::PLLSettings pll3{
		.src = RCC_Clocks::MuxSelSource::hse,
		.mult = 30, // 40,
		.refdiv = 1,
		.postdiv1 = 3, // 2,
		.postdiv2 = 1,
		.frac = 0,
	};
	static_assert(pll3.calc_freq(40'000'000) == 400'000'000);
	// static_assert(pll3.calc_freq(40'000'000) == 800'000'000);
	RCC_Clocks::set_pll<RCC_Clocks::PLL3>(pll3);

	unsigned timeout = 10'000;
	while (!RCC_Clocks::PLL3::Ready::read()) {
		if (--timeout == 0) {
			print("ERROR: PLL3 did not lock (PLL3CFGR1 = 0x", Hex{RCC->PLL3CFGR1}, ")\n");
			return false;
		}
		udelay(1);
	}
	print("PLL3 locked at 400MHz\n");

	// Now that all GPU clocks are running, pulse the GPU reset. (RM0457
	// requires the PLL3 ref clock to be slower than the GPU bus/kernel
	// clocks when GPURST is used -- true now that ck_ker_gpu is 400MHz.)
	RCC_Reset::GPU_::set();
	udelay(10);
	RCC_Reset::GPU_::clear();
	// RM0457: read back GPURST to be sure the reset has been released
	timeout = 1000;
	while (RCC_Reset::GPU_::read()) {
		if (--timeout == 0) {
			print("ERROR: GPU reset never released (GPUCFGR = 0x", Hex{RCC->GPUCFGR}, ")\n");
			return false;
		}
		udelay(1);
	}
	print("GPU reset pulsed and released\n");
	return true;
}

static void setup_rif()
{
	// Enable secure transations from GPU
	// RM0457, sec 8.3, table 37: GPU's CID is set by RISUP79
	RISC->SECCFGR[2] |= (1 << (79 - 64));
	print("RIFSC: Set SECCFGR bit\n");

	auto attr = RIMC->ATTR[9]; // RIF_MCID_GPU = 9
	RIMC->ATTR[9] = (attr & ~RIMC_ATTR_CIDSEL) | RIMC_ATTR_MSEC | RIMC_ATTR_MPRIV;
	print("RIMC ATTR[9] (GPU master) secure and privileged\n");
}

static bool ping_gpu()
{
	using namespace VivanteGpu;

	auto model = gpu_read(HI_CHIP_MODEL);

	print("Chip model:      0x", Hex{model}, "\n");
	print("Chip revision:   0x", Hex{gpu_read(HI_CHIP_REV)}, "\n");
	print("Chip date:       0x", Hex{gpu_read(HI_CHIP_DATE)}, "\n");
	print("Product ID:      0x", Hex{gpu_read(HI_CHIP_PRODUCT_ID)}, "\n");
	print("Customer ID:     0x", Hex{gpu_read(HI_CHIP_CUSTOMER_ID)}, "\n");
	print("ECO ID:          0x", Hex{gpu_read(HI_CHIP_ECO_ID)}, "\n");
	print("Identity:        0x", Hex{gpu_read(HI_CHIP_IDENTITY)}, "\n");
	print("Features:        0x", Hex{gpu_read(HI_CHIP_FEATURE)}, "\n");
	print("Minor features:  0x", Hex{gpu_read(HI_CHIP_MINOR_FEATURE_0)});
	print(" 0x", Hex{gpu_read(HI_CHIP_MINOR_FEATURE_0)});
	print(" 0x", Hex{gpu_read(HI_CHIP_MINOR_FEATURE_1)});
	print(" 0x", Hex{gpu_read(HI_CHIP_MINOR_FEATURE_2)});
	print(" 0x", Hex{gpu_read(HI_CHIP_MINOR_FEATURE_3)});
	print(" 0x", Hex{gpu_read(HI_CHIP_MINOR_FEATURE_4)});
	print(" 0x", Hex{gpu_read(HI_CHIP_MINOR_FEATURE_5)}, "\n");
	print("Specs:           0x", Hex{gpu_read(HI_CHIP_SPECS)}, "\n");
	print("Idle state:      0x", Hex{gpu_read(HI_IDLE_STATE)}, "\n");
	print("Clock control:   0x", Hex{gpu_read(HI_CLOCK_CONTROL)}, "\n");

	if (model == 0 || model == 0xFFFFFFFF) {
		print("ERROR: model register read as ", model ? "all-ones" : "zero", " -- GPU is not responding.\n");
		return false;
	}
	return true;
}

// Soft-reset the core and bring the clock to full speed
// (mirrors etnaviv_hw_reset in the Linux driver)
static bool reset_gpu_core()
{
	using namespace VivanteGpu;

	for (unsigned tries = 0; tries < 10; tries++) {
		// Load full-speed clock scale
		uint32_t control = CLK_FSCALE_VAL(0x40);
		gpu_write(HI_CLOCK_CONTROL, control | CLK_FSCALE_CMD_LOAD);
		gpu_write(HI_CLOCK_CONTROL, control);

		// Isolate, reset, release. This is a security-enabled core, so the
		// reset goes through the secure-bank MMU AHB control register --
		// HI_CLOCK_CONTROL.SOFT_RESET is for non-secure cores (we write it
		// too; it's harmless).
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

		// Let non-secure register accesses through as well (etnaviv does
		// this after reset on security-enabled cores)
		gpu_write(MMUv2_AHB_CONTROL, MMUv2_AHB_CONTROL_NONSEC_ACCESS);

		return true;
	}

	print("ERROR: GPU did not go idle after soft reset: idle = 0x", Hex{gpu_read(HI_IDLE_STATE)});
	print(" clock control = 0x", Hex{gpu_read(HI_CLOCK_CONTROL)}, "\n");
	return false;
}

// Point the FE at a command buffer in DDR and start it.
// num_dwords must be even (commands are 64-bit aligned).
//
// The FE only starts fetching on a *rising* enable: after it halts at an END
// it stays "enabled", and rewriting FE_COMMAND_CONTROL is ignored. (The
// etnaviv driver never restarts the FE -- it starts it once per reset and
// then chains buffers with WAIT/LINK.) So disable the FE first, and soft-
// reset the core to be safe -- that's the state the first kick worked from.
// Point the FE at a command buffer and start it (the arm itself; see kick_fe)
static void arm_fe(const uint32_t *buf, uint32_t num_dwords)
{
	using namespace VivanteGpu;

	gpu_write(FE_COMMAND_ADDRESS, bus_addr(buf));
	gpu_write(FE_COMMAND_CONTROL, FE_CONTROL_ENABLE | (num_dwords / 2));
	// On security-enabled cores the FE only actually starts when the enable
	// is also written through the secure bank (etnaviv_gpu_start_fe)
	gpu_write(MMUv2_SEC_COMMAND_CONTROL, FE_CONTROL_ENABLE | (num_dwords / 2));
}

static bool kick_fe(uint32_t num_dwords)
{
	using namespace VivanteGpu;

	clean_dcache_range(cmdbuf.data(), num_dwords * 4);

	gpu_write(FE_COMMAND_CONTROL, 0);
	if (!reset_gpu_core())
		return false;

	// Unmask all interrupt sources: with HI_INTR_ENBL at its reset value of
	// 0, events never latch in HI_INTR_ACKNOWLEDGE at all (etnaviv writes ~0
	// here at init). We poll ACKNOWLEDGE instead of taking the IRQ, but the
	// enable mask still gates the latch.
	gpu_write(HI_INTR_ENBL, 0xFFFFFFFF);
	gpu_read(HI_INTR_ACKNOWLEDGE); // clear stale interrupt bits

	// Note: it works without this:
	// if (!gpu_mmu_enable())
	// 	return false;

	arm_fe(cmdbuf.data(), num_dwords);
	// print("  FE armed at 0x", Hex{bus_addr(cmdbuf.data())});
	// print(", control readback 0x", Hex{gpu_read(FE_COMMAND_CONTROL)});
	// print(" sec 0x", Hex{gpu_read(MMUv2_SEC_COMMAND_CONTROL)}, "\n");
	return true;
}

static void print_fe_status(const char *msg)
{
	using namespace VivanteGpu;
	print(msg);
	print(": FE DMA addr 0x", Hex{gpu_read(FE_DMA_ADDRESS)});
	print(" status 0x", Hex{gpu_read(FE_DMA_STATUS)});
	print(" debug 0x", Hex{gpu_read(FE_DMA_DEBUG_STATE)});
	print(" last cmd 0x", Hex{gpu_read(FE_DMA_HIGH)}, Hex{gpu_read(FE_DMA_LOW)});
	print(" intr 0x", Hex{gpu_read(HI_INTR_ACKNOWLEDGE)});
	print(" axi 0x", Hex{gpu_read(HI_AXI_STATUS)});
	print(" idle 0x", Hex{gpu_read(HI_IDLE_STATE)}, "\n");
	// GPU-MMU faults (see gpu_regs.hh for the 4-bit fault codes). A fetch
	// that faults reads zeros, which looks like "executed nothing".
	print("  GPU MMU status 0x", Hex{gpu_read(MMUv2_STATUS)});
	print(" sec 0x", Hex{gpu_read(MMUv2_SEC_STATUS)});
	print(" sec fault addr 0x", Hex{gpu_read(MMUv2_SEC_EXCEPTION_ADDR)}, "\n");

	// The Illegal Access Controller latches RIF violations chip-wide (RIF
	// denials are otherwise silent: reads return 0, writes are dropped).
	// 6 x 32 sources; nonzero = something was blocked since the last check.
	// (Word 4 bit 9 = ID 137 = RISAF4, the DDR firewall.)
	auto iac_isr = reinterpret_cast<volatile uint32_t *>(IAC_BASE + 0x80);
	auto iac_icr = reinterpret_cast<volatile uint32_t *>(IAC_BASE + 0x100);
	print("  IAC violations:");
	for (unsigned i = 0; i < 6; i++) {
		print(" 0x", Hex{iac_isr[i]});
		iac_icr[i] = 0xFFFFFFFF; // clear so the next check shows fresh ones
	}
	print("\n");

	// RISAF4 captures the attributes of the transaction it rejected: the
	// address, compartment ID, and secure/privileged/write flags. IAR[0] is
	// the first illegal access since last clear, IAR[1] the most recent.
	print("  RISAF4 IASR 0x", Hex{RISAF4->IASR});
	for (unsigned i = 0; i < 2; i++) {
		auto esr = RISAF4->IAR[i].IAESR;
		print("  [", int(i), "] addr 0x", Hex{RISAF4->IAR[i].IADDR});
		print(" CID",
			  int(esr & RISAF_IAESR_IACID_Msk),
			  (esr & RISAF_IAESR_IASEC) ? " sec" : " NONSEC",
			  (esr & RISAF_IAESR_IAPRIV) ? " priv" : " unpriv",
			  (esr & RISAF_IAESR_IANRW) ? " write" : " read");
	}
	RISAF4->IACR = 0xF; // clear for next time
	print("\n  RISAF4 region1: CFGR 0x", Hex{RISAF4->REG[0].CFGR});
	print(" start 0x", Hex{RISAF4->REG[0].STARTR});
	print(" end 0x", Hex{RISAF4->REG[0].ENDR});
	print(" CIDCFGR 0x", Hex{RISAF4->REG[0].CIDCFGR}, "\n");
}

// Feed the FE a do-nothing command stream: proves the GPU can fetch from DDR
static bool test_command_fetch()
{
	using namespace VivanteGpu;

	unsigned n = 0;
	cmdbuf[n++] = CMD_NOP;
	cmdbuf[n++] = 0;
	cmdbuf[n++] = CMD_NOP;
	cmdbuf[n++] = 0;
	// The FE latches interrupt bit 1 when it executes this -- positive proof
	// the stream ran (the idle bits also read "idle" if the FE never started)
	cmdbuf[n++] = cmd_load_state(GL_EVENT);
	cmdbuf[n++] = 1 | GL_EVENT_FROM_FE;
	cmdbuf[n++] = CMD_END;
	cmdbuf[n++] = 0;

	clean_dcache_range(cmdbuf.data(), n * 4);
	if (!kick_fe(n))
		return false;

	uint32_t intr = 0;
	if (!wait_event(1 << 1, 100'000, intr)) {
		print("ERROR: FE never signaled the event (intr seen: 0x", Hex{intr}, ")\n");
		print_fe_status("  state");
		print("(If the FE DMA addr is stuck at the buffer address, the GPU never\n"
			  " fetched: kernel clock or RIMC/RISAF read-access problem)\n");
		return false;
	}

	print_fe_status("FE really executed a command stream from DDR (event received)");
	return true;
}

// Have the RS ("resolve") engine fill an RGBA8888 image in DDR, and verify
// it. The RS engine is this core's fill/copy engine: the MP25's GPU has no 2D
// pipe and -- despite what its own feature registers claim -- NO BLT engine
// (see gpu_regs.hh). This mirrors the clear that Mesa emits on this core:
// etna_rs_gen_clear_cmd() + etna_submit_rs_state() in etnaviv_rs.c.
static bool test_rs_fill()
{
	using namespace VivanteGpu;

	auto start1_tm = read_cntpct();
	image.fill(0xDEADBEEF); // so we can tell how far a partial fill got
	auto end1_tm = read_cntpct();
	print("Initial fill (cpu) takes ", end1_tm - start1_tm, " ticks\n");

	clean_dcache_range(image.data(), sizeof(image));

	auto load1 = [](uint32_t *p, uint32_t state, uint32_t value) {
		p[0] = cmd_load_state(state);
		p[1] = value;
		return p + 2;
	};

	static_assert(ImgWidth % 16 == 0, "RS requires width to be a multiple of 16");

	uint32_t *p = cmdbuf.data();
	p = load1(p, RS_CONFIG, RS_FORMAT_A8R8G8B8 | (RS_FORMAT_A8R8G8B8 << 8)); // linear, no swap
	p = load1(p, RS_SOURCE_STRIDE, 0);										 // no source: this is a fill
	p = load1(p, RS_DEST_STRIDE, ImgWidth * 4);
	p = load1(p, RS_PIPE_SOURCE_ADDR0, bus_addr(image.data())); // unused but emitted (like Mesa)
	p = load1(p, RS_PIPE_DEST_ADDR0, bus_addr(image.data()));
	p = load1(p, RS_PIPE_OFFSET0, 0);
	p = load1(p, RS_PIPE_OFFSET1, 0);
	p = load1(p, RS_WINDOW_SIZE, ImgWidth | (ImgHeight << 16));
	p = load1(p, RS_DITHER0, 0xFFFFFFFF);
	p = load1(p, RS_DITHER1, 0xFFFFFFFF);
	p = load1(p, RS_CLEAR_CONTROL, RS_CLEAR_CONTROL_ENABLED1 | 0xFFFF); // fill all 16 bit-planes
	p = load1(p, RS_FILL_VALUE0 + 0x0, ClearColor);
	p = load1(p, RS_FILL_VALUE0 + 0x4, ClearColor);
	p = load1(p, RS_FILL_VALUE0 + 0x8, ClearColor);
	p = load1(p, RS_FILL_VALUE0 + 0xC, ClearColor);
	p = load1(p, RS_EXTRA_CONFIG, 0);
	p = load1(p, RS_SINGLE_BUFFER, 1); // this core has the SINGLE_BUFFER feature
	p = load1(p, RS_KICKER, RS_KICK);  // magic value starts the resolve
	p = load1(p, RS_SINGLE_BUFFER, 0);

	// The RS runs as part of the PE pipeline: stall the FE until the PE is
	// done, flush the color cache to memory, and stall again
	auto stall_fe_on_pe = [&]() {
		p = load1(p, GL_SEMAPHORE_TOKEN, sync_token(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE));
		*p++ = CMD_STALL;
		*p++ = sync_token(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
	};
	stall_fe_on_pe();
	p = load1(p, GL_FLUSH_CACHE, GL_FLUSH_CACHE_COLOR | GL_FLUSH_CACHE_DEPTH);
	stall_fe_on_pe();

	// PE latches interrupt bit 2 when the fill (and flush) is complete;
	// the FE latches bit 3 when it gets here, past all the stalls
	p = load1(p, GL_EVENT, 2 | GL_EVENT_FROM_PE);
	p = load1(p, GL_EVENT, 3 | GL_EVENT_FROM_FE);
	*p++ = CMD_END;
	*p++ = 0;

	clean_dcache_range(cmdbuf.data(), (p - cmdbuf.data()) * 4);

	// Start measuring how long it takes to fill an area with the GPU:
	auto start_tm = read_cntpct();

	if (!kick_fe(p - cmdbuf.data()))
		return false;

// #define USE_POLLING
#ifdef USE_POLLING
	uint32_t intr = 0;
	bool done = wait_event((1 << 3) | (1 << 2), 100'000, intr); // 103000 ticks
	auto end_tm = read_cntpct();
#else
	uint32_t intr = 0;
	// std::atomic<bool> done = false;
	bool done = false;
	std::atomic<uint32_t> end_tm = 0;
	std::atomic<uint32_t> end_tm2 = 0;

	InterruptManager::register_and_start_isr(GPU_IRQn, 0, 0, [&]() {
		auto ack = gpu_read(VivanteGpu::HI_INTR_ACKNOWLEDGE);
		if (ack & (VivanteGpu::INTR_AXI_BUS_ERROR | VivanteGpu::INTR_MMU_EXCEPTION)) {
			end_tm = read_cntpct();
		}
		if (ack & VivanteGpu::INTR_FROM_PE) {
			end_tm = read_cntpct();
		} else if (ack & VivanteGpu::INTR_FROM_FE) {
			end_tm2 = read_cntpct();
		}
	});

	uint32_t timeout = 100000;
	while (end_tm == 0) { // done.load(std::memory_order_acquire) == false) {
		if (--timeout == 0) {
			break;
		}
		asm("wfe");
	}
	done = timeout > 0;
#endif

	if (done) {
		print("RS fill event PE:", end_tm - start_tm, " ticks, FE:", end_tm2 - start_tm, " ticks\n");
	} else {
		print("RS fill event: ", (intr & (1 << 2)) ? "received" : "MISSING");
		print(", end-of-stream event: ", (intr & (1 << 3)) ? "received" : "MISSING");
		print(" (intr seen: 0x", Hex{intr}, ")\n");
		print_fe_status("  state");
		if (!wait_idle(IDLE_FE | IDLE_PE, 100'000))
			print("  FE/PE are not idle: the stream is stuck\n");
		return false;
	}

	// The GPU wrote to DDR behind the CPU's back: drop our stale cache lines
	invalidate_dcache_range(image.data(), sizeof(image));

	unsigned wrong = 0;
	unsigned first_wrong = 0;
	for (auto i = 0u; i < image.size(); i++) {
		if (image[i] != ClearColor) {
			if (wrong == 0)
				first_wrong = i;
			wrong++;
		}
	}

	if (wrong) {
		print("ERROR: ", int(wrong), " of ", int(image.size()));
		print(" pixels wrong. First wrong pixel [", int(first_wrong), "] ");
		print("= 0x", Hex{image[first_wrong]}, "\n");
		return false;
	}

	print("\n");
	print("GPU filled a ", int(ImgWidth), "x", int(ImgHeight));
	print(" RGBA image with 0x", Hex{ClearColor}, " -- verified by CPU. \\o/\n");
	return true;
}

int main()
{
	print("\nGPU Example\n");
	print("===========\n");

	SystemA35_SYSTICK_Config(0); // init the generic timer so udelay() works

	print("\n1) Powering VDDGPU\n");
	// The stm32mp2-baremetal TF-A fork enables the GPU rail (PMIC buck3)
	// during boot, so normally the supply monitor sees it immediately.
	bool ok = power_up_vddgpu(10'000);
	if (!ok) {
		print("Your TF-A does not enable buck3 -- trying to enable it over I2C7...\n");
		ok = enable_buck3_on_pmic() && power_up_vddgpu(100'000);
	}

	if (ok) {
		// RIF first: the master attributes should be in place before the
		// GPU's bus interface ever comes out of reset
		print("\n2) Configuring RIF for the GPU\n");
		setup_rif();

		print("\n3) Clocking GPU from PLL3\n");
		ok = clock_and_reset_gpu();
	}

	if (ok) {
		print("\n4) Pinging the GPU (chip identification)\n");
		ok = ping_gpu();
	}

	if (ok) {
		print("\n5) Soft-resetting the GPU core\n");
		ok = reset_gpu_core();
	}

	if (ok) {
		print("\n6) Running a NOP command stream (GPU fetches from DDR)\n");
		ok = test_command_fetch();
	}

	if (ok) {
		print("\n7) Filling a buffer using the RS (resolve) engine\n");
		ok = test_rs_fill();
	}

	print(ok ? "\nSUCCESS\n" : "\nFAILED (see above)\n");

	while (true) {
		asm volatile("wfe");
	}
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", int(line), "\n");
}
