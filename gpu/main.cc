#include "aarch64/system_reg.hh"
#include "drivers/pin.hh"
#include "drivers/rcc.hh"
#include "drivers/rcc_pll.hh"
#include "drivers/rcc_xbar.hh"
#include "gpu_regs.hh"
#include "print/print.hh"
#include "stm32mp2xx_hal.h"
#include <array>
#include <cstdint>
#include <cstring>

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

void clean_dcache_range(const void *start, size_t bytes)
{
	auto addr = reinterpret_cast<uintptr_t>(start);
	for (auto a = addr & ~63u; a < addr + bytes; a += 64)
		clean_dcache_address(a);
	dsb_sy();
}

void invalidate_dcache_range(const void *start, size_t bytes)
{
	auto addr = reinterpret_cast<uintptr_t>(start);
	for (auto a = addr & ~63u; a < addr + bytes; a += 64)
		invalidate_dcache_address(a);
	dsb_sy();
}

// The GPU fetches commands and reads/writes images in DDR
alignas(64) std::array<uint32_t, 128> cmdbuf;

constexpr uint32_t ImgWidth = 64;
constexpr uint32_t ImgHeight = 64;
constexpr uint32_t ClearColor = 0x4D5A11AC;
alignas(64) std::array<uint32_t, ImgWidth * ImgHeight> image;

bool wait_idle(uint32_t idle_bits, unsigned timeout_us)
{
	while (timeout_us--) {
		if ((gpu_read(VivanteGpu::HI_IDLE_STATE) & idle_bits) == idle_bits)
			return true;
		udelay(1);
	}
	return false;
}

} // namespace

// VDDGPU comes from the STPMIC25's buck3 on the EV1. Nothing in the boot chain
// enables it ("regulator-always-on" in the TF-A device tree only *prevents
// disabling* -- TF-A BL2 registers PMIC regulators but never enables unused
// ones), so we must switch it on ourselves, over I2C7 (PD15=SCL, PD14=SDA).
static bool enable_buck3_on_pmic()
{
	constexpr uint16_t PmicAddr = 0x33 << 1; // STPMIC25, 7-bit address 0x33
	constexpr uint8_t ProductIdReg = 0x00;
	constexpr uint8_t VersionReg = 0x01;
	constexpr uint8_t Buck3MainCr1 = 0x2A; // voltage: (500 + 10 * n) mV
	constexpr uint8_t Buck3MainCr2 = 0x2B; // bit 0: enable
	constexpr uint8_t Buck3_800mV = 30;

	// I2C7 kernel clock: PLL7 at 80 MHz into flexgen channel 15
	// (same clock setup the i2c example uses for I2C1/2)
	constexpr RCC_Clocks::PLLSettings pll7{
		.src = RCC_Clocks::MuxSelSource::hse,
		.mult = 2,
		.refdiv = 1,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.frac = 0,
	};
	static_assert(pll7.calc_freq(40'000'000) == 80'000'000);
	RCC_Clocks::set_pll<RCC_Clocks::PLL7>(pll7);
	FlexbarConf i2c_xbar{.PLL = FlexbarConf::PLLx::_7, .findiv = 0x3F, .prediv = 0};
	i2c_xbar.init(15); // flexgen channel 15 = ck_ker_i2c7

	__HAL_RCC_I2C7_CLK_ENABLE();

	Pin scl{GPIO::D,
			PinNum::_15,
			PinMode::Alt,
			AltFunc10,
			PinPull::None,
			PinPolarity::Normal,
			PinSpeed::Medium,
			PinOType::OpenDrain};
	Pin sda{GPIO::D,
			PinNum::_14,
			PinMode::Alt,
			AltFunc10,
			PinPull::None,
			PinPolarity::Normal,
			PinSpeed::Medium,
			PinOType::OpenDrain};

	static I2C_HandleTypeDef hi2c;
	memset(&hi2c, 0, sizeof hi2c);
	hi2c.Instance = I2C7;
	hi2c.Init.Timing = 0x10702525; // about 100kHz (same kernel clock as i2c example)
	hi2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	hi2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;

	if (auto res = HAL_I2C_Init(&hi2c); res != HAL_OK) {
		print("ERROR: HAL_I2C_Init returned ", res, "\n");
		return false;
	}

	auto pmic_read = [&](uint8_t reg, uint8_t &val) {
		return HAL_I2C_Mem_Read(&hi2c, PmicAddr, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
	};
	auto pmic_write = [&](uint8_t reg, uint8_t val) {
		return HAL_I2C_Mem_Write(&hi2c, PmicAddr, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
	};

	uint8_t product_id = 0, version = 0;
	if (auto res = pmic_read(ProductIdReg, product_id); res != HAL_OK) {
		print("ERROR: cannot reach PMIC on I2C7 (HAL_I2C_Mem_Read returned ", res, ")\n");
		return false;
	}
	pmic_read(VersionReg, version);
	print("PMIC product ID 0x", Hex{product_id}, ", version 0x", Hex{version}, "\n");

	uint8_t cr1 = 0, cr2 = 0;
	pmic_read(Buck3MainCr1, cr1);
	pmic_read(Buck3MainCr2, cr2);
	print("Buck3 (VDDGPU) was: voltage code ", cr1, ", control 0x", Hex{cr2}, "\n");

	if (auto res = pmic_write(Buck3MainCr1, Buck3_800mV); res != HAL_OK) {
		print("ERROR: PMIC write failed (", res, ")\n");
		return false;
	}
	if (auto res = pmic_write(Buck3MainCr2, cr2 | 1); res != HAL_OK) {
		print("ERROR: PMIC write failed (", res, ")\n");
		return false;
	}
	udelay(2000); // buck ramp: stpmic2 driver uses ~1ms enable ramp delay

	pmic_read(Buck3MainCr1, cr1);
	pmic_read(Buck3MainCr2, cr2);
	print("Buck3 (VDDGPU) now: voltage code ", cr1, " (800mV), control 0x", Hex{cr2}, "\n");
	return true;
}

static bool power_up_vddgpu()
{
	// Even with buck3 up, the GPU domain is electrically isolated until
	// software confirms the supply with the PWR voltage monitor and sets
	// the "supply valid" bit (like the other independent supplies, see
	// drivers/pwr_vdd.hh).
	print("PWR CR12 = 0x", Hex{PWR->CR12}, "\n");

	PWR->CR12 |= PWR_CR12_GPUVMEN;
	unsigned timeout_us = 100'000;
	while (!(PWR->CR12 & PWR_CR12_VDDGPURDY)) {
		if (--timeout_us == 0) {
			print("ERROR: VDDGPU never became ready (CR12 = 0x", Hex{PWR->CR12}, ")\n");
			print("Check VDDGPU with a meter -- it should be 0.80V\n");
			return false;
		}
		udelay(1);
	}
	PWR->CR12 &= ~PWR_CR12_GPUVMEN;
	PWR->CR12 |= PWR_CR12_GPUSV;
	print("VDDGPU is valid (CR12 = 0x", Hex{PWR->CR12}, ")\n");
	return true;
}

static void clock_and_reset_gpu()
{
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
	while (!RCC_Clocks::PLL3::Ready::read())
		;
	print("PLL3 locked at 400MHz\n");

	// Hold the GPU in reset while enabling its clocks, then release
	RCC_Reset::GPU_::set();
	RCC_Enable::GPU_::set();
	udelay(10);
	RCC_Reset::GPU_::clear();
	udelay(10);
	print("GPU clock enabled, reset released (RCC GPUCFGR = 0x", Hex{RCC->GPUCFGR}, ")\n");
}

static void setup_rif()
{
	// Slave side: the GPU's AHB register port. RIF peripherals reset to
	// non-secure, which still grants our secure (EL3) accesses, so nothing
	// needs to change -- just show the state (GPU is RIFSC peripheral 79,
	// bit 15 of SECCFGR[2]).
	print("RIFSC: GPU periph SECCFGR2 = 0x",
		  Hex{RISC->SECCFGR[2]},
		  ", CIDCFGR = 0x",
		  Hex{RISC->PER[79].CIDCFGR},
		  "\n");

	// Master side: transactions the GPU issues to DDR pass through RISAF4,
	// which TF-A configured to allow only *secure* CID0/CID1 masters. The
	// GPU's RIMC entry resets to non-secure, so its DDR reads/writes would
	// be silently dropped. Make it issue secure privileged transactions
	// (its CID stays 0, which RISAF4 permits).
	auto attr = RIMC->ATTR[9]; // RIF_MCID_GPU = 9
	RIMC->ATTR[9] = attr | RIMC_ATTR_MSEC | RIMC_ATTR_MPRIV;
	print("RIMC ATTR[9] (GPU master): 0x", Hex{attr}, " => 0x", Hex{RIMC->ATTR[9]}, "\n");
}

static bool ping_gpu()
{
	using namespace VivanteGpu;

	auto model = gpu_read(HI_CHIP_MODEL);
	auto rev = gpu_read(HI_CHIP_REV);

	print("Chip model:      0x", Hex{model}, "\n");
	print("Chip revision:   0x", Hex{rev}, "\n");
	print("Chip date:       0x", Hex{gpu_read(HI_CHIP_DATE)}, "\n");
	print("Product ID:      0x", Hex{gpu_read(HI_CHIP_PRODUCT_ID)}, "\n");
	print("Customer ID:     0x", Hex{gpu_read(HI_CHIP_CUSTOMER_ID)}, "\n");
	print("ECO ID:          0x", Hex{gpu_read(HI_CHIP_ECO_ID)}, "\n");
	print("Identity:        0x", Hex{gpu_read(HI_CHIP_IDENTITY)}, "\n");
	print("Features:        0x", Hex{gpu_read(HI_CHIP_FEATURE)}, "\n");
	print("Minor features:  0x",
		  Hex{gpu_read(HI_CHIP_MINOR_FEATURE_0)},
		  " 0x",
		  Hex{gpu_read(HI_CHIP_MINOR_FEATURE_1)},
		  " 0x",
		  Hex{gpu_read(HI_CHIP_MINOR_FEATURE_2)},
		  " 0x",
		  Hex{gpu_read(HI_CHIP_MINOR_FEATURE_3)},
		  " 0x",
		  Hex{gpu_read(HI_CHIP_MINOR_FEATURE_4)},
		  " 0x",
		  Hex{gpu_read(HI_CHIP_MINOR_FEATURE_5)},
		  "\n");
	print("Specs:           0x", Hex{gpu_read(HI_CHIP_SPECS)}, "\n");
	print("Idle state:      0x", Hex{gpu_read(HI_IDLE_STATE)}, "\n");
	print("Clock control:   0x", Hex{gpu_read(HI_CLOCK_CONTROL)}, "\n");

	if (model == 0 || model == 0xFFFFFFFF) {
		print("ERROR: identity registers read as ", model ? "all-ones" : "zero", " -- GPU is not responding.\n");
		print("(A silent zero-read usually means a RIF/clock/power problem, see README)\n");
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

		// Isolate, soft reset, release
		control |= CLK_ISOLATE_GPU;
		gpu_write(HI_CLOCK_CONTROL, control);
		gpu_write(HI_CLOCK_CONTROL, control | CLK_SOFT_RESET);
		udelay(20);
		gpu_write(HI_CLOCK_CONTROL, control);
		control &= ~CLK_ISOLATE_GPU;
		gpu_write(HI_CLOCK_CONTROL, control);

		if (!(gpu_read(HI_IDLE_STATE) & IDLE_FE))
			continue;
		if (!(gpu_read(HI_CLOCK_CONTROL) & CLK_IDLE_3D))
			continue;

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
static void kick_fe(uint32_t num_dwords)
{
	using namespace VivanteGpu;

	clean_dcache_range(cmdbuf.data(), num_dwords * 4);

	gpu_read(HI_INTR_ACKNOWLEDGE); // clear stale interrupt bits

	gpu_write(FE_COMMAND_ADDRESS, bus_addr(cmdbuf.data()));
	gpu_write(FE_COMMAND_CONTROL, FE_CONTROL_ENABLE | (num_dwords / 2));
}

static void print_fe_status(const char *msg)
{
	using namespace VivanteGpu;
	print(msg,
		  ": FE DMA addr 0x",
		  Hex{gpu_read(FE_DMA_ADDRESS)},
		  " status 0x",
		  Hex{gpu_read(FE_DMA_STATUS)},
		  " debug 0x",
		  Hex{gpu_read(FE_DMA_DEBUG_STATE)},
		  " last cmd 0x",
		  Hex{gpu_read(FE_DMA_HIGH)},
		  Hex{gpu_read(FE_DMA_LOW)},
		  " intr 0x",
		  Hex{gpu_read(HI_INTR_ACKNOWLEDGE)},
		  " axi 0x",
		  Hex{gpu_read(HI_AXI_STATUS)},
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
	cmdbuf[n++] = CMD_END;
	cmdbuf[n++] = 0;

	kick_fe(n);

	if (!wait_idle(IDLE_FE, 100'000)) {
		print_fe_status("ERROR: FE did not go idle running a NOP stream");
		print("(FE DMA addr stuck at the buffer address usually means the GPU's\n"
			  " DDR reads are blocked -- check the RIMC/RISAF config)\n");
		return false;
	}

	print_fe_status("FE executed a NOP command stream from DDR");
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
	p = load1(p, BLT_ENABLE, 0);
	// FE waits until the BLT engine is done (BLT must be enabled around
	// semaphore/stall commands that target it)
	p = load1(p, BLT_ENABLE, 1);
	p = load1(p, GL_SEMAPHORE_TOKEN, sync_token(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_BLT));
	*p++ = CMD_STALL;
	*p++ = sync_token(SYNC_RECIPIENT_FE, SYNC_RECIPIENT_BLT);
	p = load1(p, BLT_ENABLE, 0);
	*p++ = CMD_END;
	*p++ = 0;

	kick_fe(p - cmdbuf.data());

	if (!wait_idle(IDLE_FE | IDLE_BLT, 100'000)) {
		print_fe_status("ERROR: FE/BLT did not go idle");
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

	print("\n1) Enabling VDDGPU on the PMIC (buck3, via I2C7)\n");
	bool ok = enable_buck3_on_pmic();

	if (ok) {
		print("\n2) Removing VDDGPU power isolation\n");
		ok = power_up_vddgpu();
	}

	if (ok) {
		print("\n3) Clocking GPU from PLL3\n");
		clock_and_reset_gpu();

		print("\n4) Configuring RIF for the GPU\n");
		setup_rif();

		print("\n5) Pinging the GPU (chip identification)\n");
		ok = ping_gpu();
	}

	if (ok) {
		print("\n6) Soft-resetting the GPU core\n");
		ok = reset_gpu_core();
	}

	if (ok) {
		print("\n7) Running a NOP command stream (GPU fetches from DDR)\n");
		ok = test_command_fetch();
	}

	if (ok) {
		print("\n8) Filling a buffer using the BLT engine\n");
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
