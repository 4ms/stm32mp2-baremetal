#include "aarch64/system_reg.hh"
#include "drivers/rcc.hh"
#include "drivers/rcc_pll.hh"
#include "gpu_regs.hh"
#include "print/print.hh"
#include "stm32mp2xx.h"
#include <array>
#include <cstdint>

bool enable_buck3_on_pmic();

// GPU bring-up example.
//
// The MP25x has a VeriSilicon (Vivante) GCNanoUltra31 GPU at GPU_BASE
// (0x48280000). This example brings it up from a cold state and talks to it at
// the register level (there is no vendor driver in a baremetal context):
//
//   1) Power:  VDDGPU (PMIC buck3 on the EV1, always-on in the TF-A BL2 device
//      tree) is validated with the PWR supply monitor.
//   2) Clock:  PLL3 is the GPU kernel clock. RCC_GPUCFGR gates/resets the GPU.
//   3) RIF:    the GPU is also a bus *master*. The DDR firewall (RISAF4) is
//      configured by TF-A to allow secure CID0/CID1 masters only, so the RIMC
//      entry for the GPU must be set to issue secure transactions.
//   4) Ping:   read the chip identity registers and check the core goes idle.
//   5) FE:     run a trivial command stream (NOPs + END) from DDR through the
//      command-stream front end -- first proof the GPU can master the bus.
//   6) Fill:   use the BLT engine (the halti5-era blitter) to clear a small
//      RGBA8888 image in DDR to a solid color, then verify it with the CPU.

extern "C" uint32_t SystemA35_SYSTICK_Config(uint32_t);
extern "C" void udelay(unsigned us);

namespace
{

volatile uint32_t *const gpu = reinterpret_cast<volatile uint32_t *>(GPU_BASE);

uint32_t gpu_read(uint32_t offset)
{
	return gpu[offset / 4];
}

void gpu_write(uint32_t offset, uint32_t value)
{
	gpu[offset / 4] = value;
}

// GPU addresses are 32-bit bus addresses. We run with a flat (virt == phys)
// MMU mapping, so a pointer is already a bus address.
uint32_t bus_addr(const void *p)
{
	return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

// Non-secure GPU region setup in mmu.cc:
constexpr uintptr_t GpuBufBase = 0x8A200000;
constexpr uintptr_t GpuBufSize = 0x200000;

auto &cmdbuf = *reinterpret_cast<std::array<uint32_t, 128> *>(GpuBufBase);

constexpr uint32_t ImgWidth = 64;
constexpr uint32_t ImgHeight = 64;
constexpr uint32_t ClearColor = 0x4D5A11AC;
auto &image = *reinterpret_cast<std::array<uint32_t, ImgWidth * ImgHeight> *>(GpuBufBase + 0x1000);

bool wait_idle(uint32_t idle_bits, unsigned timeout_us)
{
	while (timeout_us--) {
		if ((gpu_read(VivanteGpu::HI_IDLE_STATE) & idle_bits) == idle_bits)
			return true;
		udelay(1);
	}
	return false;
}

// Wait for event bits to latch in HI_INTR_ACKNOWLEDGE. Unlike the idle bits
// (which also read "idle" if the FE never started!), an event is positive
// proof the command stream executed up to the EVENT command.
// Reading the register clears it, so accumulate into intr_acc.
bool wait_event(uint32_t event_mask, unsigned timeout_us, uint32_t &intr_acc)
{
	while (timeout_us--) {
		intr_acc |= gpu_read(VivanteGpu::HI_INTR_ACKNOWLEDGE);
		if ((intr_acc & event_mask) == event_mask)
			return true;
		udelay(1);
	}
	return false;
}

} // namespace

// Even with buck3 up, the GPU domain is electrically isolated until software
// confirms the supply with the PWR voltage monitor and sets the "supply valid"
// bit (like the other independent supplies, see drivers/pwr_vdd.hh).
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
	// The monitor must STAY enabled: its live output releases the GPU power
	// domain reset (vgpu_rstn, RM0457 fig. 90). Disabling it -- like
	// pwr_vdd.hh does for the VDDIO supplies -- keeps the whole GPU domain
	// (including PLL3's registers!) in reset, and accessing them hangs the bus.
	PWR->CR12 |= PWR_CR12_GPUSV;
	print("VDDGPU is valid (CR12 = 0x", Hex{PWR->CR12}, ")\n");
	return true;
}

static bool clock_and_reset_gpu()
{
	// The PLL3 hardware physically lives inside the GPU subsystem (RM0457
	// fig. 100: the RCC only sends it a reference clock, ck_gpuss_pll3_fref),
	// so the GPU must be powered and its bus clocks enabled *before* PLL3 can
	// be programmed and lock. The GPU comes out of power-on not in reset, so
	// no GPURST release is needed first.
	RCC_Enable::GPU_::set();
	udelay(10);
	print("GPU clock enabled (RCC GPUCFGR = 0x", Hex{RCC->GPUCFGR}, ")\n");

	// PLL3 is dedicated to the GPU (no flexgen channel involved).
	// 40 MHz HSE / 1 * 30 = 1200 MHz VCO, / 3 = 400 MHz.
	// 400 MHz is a safe speed for the 0.80 V VDDGPU the PMIC provides.
	constexpr RCC_Clocks::PLLSettings pll3{
		.src = RCC_Clocks::MuxSelSource::hse,
		.mult = 30,
		.refdiv = 1,
		.postdiv1 = 3,
		.postdiv2 = 1,
		.frac = 0,
	};
	static_assert(pll3.calc_freq(40'000'000) == 400'000'000);
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
	print("RIFSC: GPU periph SECCFGR2 = 0x", Hex{RISC->SECCFGR[2]}, ", CIDCFGR = 0x", Hex{RISC->PER[79].CIDCFGR}, "\n");

	auto attr = RIMC->ATTR[9]; // RIF_MCID_GPU = 9
	RIMC->ATTR[9] = (attr & ~RIMC_ATTR_CIDSEL) | RIMC_ATTR_MSEC | RIMC_ATTR_MPRIV;
	print("RIMC ATTR[9] (GPU master): 0x", Hex{attr}, " => 0x", Hex{RIMC->ATTR[9]}, "\n");
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

		print("GPU core soft-reset OK (idle = 0x", Hex{gpu_read(HI_IDLE_STATE)}, ")\n");
		return true;
	}

	print("ERROR: GPU did not go idle after soft reset: idle = 0x",
		  Hex{gpu_read(HI_IDLE_STATE)},
		  " clock control = 0x",
		  Hex{gpu_read(HI_CLOCK_CONTROL)},
		  "\n");
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

	gpu_write(FE_COMMAND_ADDRESS, bus_addr(cmdbuf.data()));
	gpu_write(FE_COMMAND_CONTROL, FE_CONTROL_ENABLE | (num_dwords / 2));
	// On security-enabled cores the FE only actually starts when the enable
	// is also written through the secure bank (etnaviv_gpu_start_fe)
	gpu_write(MMUv2_SEC_COMMAND_CONTROL, FE_CONTROL_ENABLE | (num_dwords / 2));
	print("  FE armed at 0x",
		  Hex{bus_addr(cmdbuf.data())},
		  ", control readback 0x",
		  Hex{gpu_read(FE_COMMAND_CONTROL)},
		  " sec 0x",
		  Hex{gpu_read(MMUv2_SEC_COMMAND_CONTROL)},
		  "\n");
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
	print(" axi 0x", Hex{gpu_read(HI_AXI_STATUS)}, "\n");
	// GPU-MMU faults (see gpu_regs.hh for the 4-bit fault codes). A fetch
	// that faults reads zeros, which looks like "executed nothing".
	print("  GPU MMU status 0x", Hex{gpu_read(MMUv2_STATUS)});
	print(" sec 0x", Hex{gpu_read(MMUv2_SEC_STATUS)});
	print(" sec fault addr 0x", Hex{gpu_read(MMUv2_SEC_EXCEPTION_ADDR)}, "\n");

	// The Illegal Access Controller latches RIF violations chip-wide (RIF
	// denials are otherwise silent: reads return 0, writes are dropped).
	// 6 x 32 sources; nonzero = something was blocked since the last check.
	// (Word 4 bit 9 = ID 137 = RISAF4, the DDR firewall.)
	volatile uint32_t *iac_isr = reinterpret_cast<volatile uint32_t *>(IAC_BASE + 0x80);
	volatile uint32_t *iac_icr = reinterpret_cast<volatile uint32_t *>(IAC_BASE + 0x100);
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
		print("  [",
			  int(i),
			  "] addr 0x",
			  Hex{RISAF4->IAR[i].IADDR},
			  " CID",
			  int(esr & RISAF_IAESR_IACID_Msk),
			  (esr & RISAF_IAESR_IASEC) ? " sec" : " NONSEC",
			  (esr & RISAF_IAESR_IAPRIV) ? " priv" : " unpriv",
			  (esr & RISAF_IAESR_IANRW) ? " write" : " read");
	}
	RISAF4->IACR = 0xF; // clear for next time
	print("\n  RISAF4 region1: CFGR 0x",
		  Hex{RISAF4->REG[0].CFGR},
		  " start 0x",
		  Hex{RISAF4->REG[0].STARTR},
		  " end 0x",
		  Hex{RISAF4->REG[0].ENDR},
		  " CIDCFGR 0x",
		  Hex{RISAF4->REG[0].CIDCFGR},
		  "\n");
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

// Have the BLT engine fill (clear) an RGBA8888 image in DDR, and verify it
static bool test_blt_fill()
{
	using namespace VivanteGpu;

	image.fill(0xDEADBEEF); // so we can tell how far a partial fill got
	clean_dcache_range(image.data(), sizeof(image));

	auto load1 = [](uint32_t *p, uint32_t state, uint32_t value) {
		p[0] = cmd_load_state(state);
		p[1] = value;
		return p + 2;
	};

	// The clear-image sequence the Mesa etnaviv driver emits (etnaviv_blt.c),
	// followed by a FE-waits-for-BLT semaphore+stall so that FE idle also
	// means BLT done, then END.
	uint32_t *p = cmdbuf.data();
	p = load1(p, BLT_ENABLE, 1);
	p = load1(p, BLT_CONFIG, blt_config_clear_bpp(4));
	p = load1(p, BLT_DEST_STRIDE, blt_stride(ImgWidth * 4));
	p = load1(p, BLT_DEST_CONFIG, BLT_IMAGE_CONFIG_PLAIN);
	p = load1(p, BLT_DEST_ADDR, bus_addr(image.data()));
	p = load1(p, BLT_SRC_STRIDE, blt_stride(ImgWidth * 4));
	p = load1(p, BLT_SRC_CONFIG, BLT_IMAGE_CONFIG_PLAIN);
	p = load1(p, BLT_SRC_ADDR, bus_addr(image.data()));
	p = load1(p, BLT_DEST_POS, 0); // x=0, y=0
	p = load1(p, BLT_IMAGE_SIZE, ImgWidth | (ImgHeight << 16));
	p = load1(p, BLT_CLEAR_COLOR0, ClearColor);
	p = load1(p, BLT_CLEAR_COLOR1, ClearColor);
	p = load1(p, BLT_CLEAR_BITS0, 0xFFFFFFFF);
	p = load1(p, BLT_CLEAR_BITS1, 0xFFFFFFFF);
	p = load1(p, BLT_SET_COMMAND, 0x3);
	p = load1(p, BLT_COMMAND, BLT_COMMAND_CLEAR_IMAGE);
	p = load1(p, BLT_SET_COMMAND, 0x3);
	// BLT latches interrupt bit 2 when the clear has completed
	p = load1(p, GL_EVENT, 2 | GL_EVENT_FROM_BLT);
	p = load1(p, BLT_ENABLE, 0);
	// FE waits until the BLT engine is done (BLT must be enabled around
	// semaphore/stall commands that target it)
	auto stall_fe_on = [&](uint32_t recipient) {
		if (recipient == SYNC_RECIPIENT_BLT)
			p = load1(p, BLT_ENABLE, 1);
		p = load1(p, GL_SEMAPHORE_TOKEN, sync_token(SYNC_RECIPIENT_FE, recipient));
		*p++ = CMD_STALL;
		*p++ = sync_token(SYNC_RECIPIENT_FE, recipient);
		if (recipient == SYNC_RECIPIENT_BLT)
			p = load1(p, BLT_ENABLE, 0);
	};
	stall_fe_on(SYNC_RECIPIENT_BLT);

	// Flush the GPU-internal caches to memory, the way the etnaviv driver
	// ends every 3D-pipe command buffer: without this, the cleared pixels
	// can stay in the GPU's write-back caches and never reach DDR.
	stall_fe_on(SYNC_RECIPIENT_PE);
	p = load1(p, GL_FLUSH_CACHE, GL_FLUSH_CACHE_PIPE3D);
	p = load1(p, BLT_ENABLE, 1);
	p = load1(p, BLT_SET_COMMAND, 0x1); // BLT cache flush
	p = load1(p, BLT_ENABLE, 0);
	stall_fe_on(SYNC_RECIPIENT_PE);
	stall_fe_on(SYNC_RECIPIENT_BLT);

	// FE latches interrupt bit 3 when it gets here -- past all the stalls,
	// so the clear and the cache flushes are complete
	p = load1(p, GL_EVENT, 3 | GL_EVENT_FROM_FE);
	*p++ = CMD_END;
	*p++ = 0;

	if (!kick_fe(p - cmdbuf.data()))
		return false;

	uint32_t intr = 0;
	bool done = wait_event(1 << 3, 100'000, intr);
	print("BLT clear event: ",
		  (intr & (1 << 2)) ? "received" : "MISSING",
		  ", end-of-stream event: ",
		  done ? "received" : "MISSING",
		  " (intr seen: 0x",
		  Hex{intr},
		  ")\n");
	if (!done) {
		print_fe_status("  state");
		if (!wait_idle(IDLE_FE | IDLE_BLT, 100'000))
			print("  FE/BLT are not idle: the stream is stuck (semaphore never signaled?)\n");
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
		print("ERROR: ",
			  int(wrong),
			  " of ",
			  int(image.size()),
			  " pixels wrong. First wrong pixel [",
			  int(first_wrong),
			  "] = 0x",
			  Hex{image[first_wrong]},
			  "\n");
		return false;
	}

	print("GPU filled a ",
		  int(ImgWidth),
		  "x",
		  int(ImgHeight),
		  " RGBA image with 0x",
		  Hex{ClearColor},
		  " -- verified by CPU. \\o/\n");
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
		print("\n7) Filling a buffer using the BLT engine\n");
		ok = test_blt_fill();
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
