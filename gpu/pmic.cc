#include "drivers/pin.hh"
#include "drivers/rcc.hh"
#include "drivers/rcc_pll.hh"
#include "drivers/rcc_xbar.hh"
#include "print/print.hh"
#include <cstdint>
#include <cstring>

extern "C" void udelay(unsigned us);

// VDDGPU comes from the STPMIC25's buck3 on the EV1. Nothing in the boot chain
// enables it ("regulator-always-on" in the TF-A device tree only *prevents
// disabling* -- TF-A BL2 registers PMIC regulators but never enables unused
// ones), so we must switch it on ourselves, over I2C7 (PD15=SCL, PD14=SDA).
bool enable_buck3_on_pmic()
{
	constexpr uint16_t PmicAddr = 0x33 << 1; // STPMIC25, 7-bit address 0x33
	constexpr uint8_t ProductIdReg = 0x00;
	constexpr uint8_t VersionReg = 0x01;
	constexpr uint8_t Buck3MainCr1 = 0x2A; // voltage: (500 + 10 * n) mV
	constexpr uint8_t Buck3MainCr2 = 0x2B; // bit 0: enable
	constexpr uint8_t Buck3_800mV = 30;

	// I2C7 kernel clock: PLL7 at 80 MHz into flexgen channel 15
	// (same clock setup the i2c example uses for I2C1/2)
	print("flexgen ch15 was: XBAR 0x",
		  Hex{RCC->XBARxCFGR[15]},
		  " FINDIV 0x",
		  Hex{RCC->FINDIVxCFGR[15]},
		  " PREDIV 0x",
		  Hex{RCC->PREDIVxCFGR[15]},
		  "\n");
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
	while (!RCC_Clocks::PLL7::Ready::read())
		;
	FlexbarConf i2c_xbar{.PLL = FlexbarConf::PLLx::_7, .findiv = 0x3F, .prediv = 0};
	i2c_xbar.init(15); // flexgen channel 15 = ck_ker_i2c7
	print("flexgen ch15 now: XBAR 0x", Hex{RCC->XBARxCFGR[15]}, " FINDIV 0x", Hex{RCC->FINDIVxCFGR[15]}, "\n");

	__HAL_RCC_I2C7_CLK_ENABLE();
	print("RCC I2C7CFGR = 0x", Hex{RCC->I2C7CFGR}, "\n");

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

	// SDA and SCL should both idle high (pull-ups to the PMIC's VIO rail)
	print("I2C7 pin levels: SCL(PD15)=", int((GPIOD->IDR >> 15) & 1), " SDA(PD14)=", int((GPIOD->IDR >> 14) & 1), "\n");

	if (auto res = HAL_I2C_Init(&hi2c); res != HAL_OK) {
		print("ERROR: HAL_I2C_Init returned ", res, "\n");
		return false;
	}
	// If these read back 0, our register writes are being silently dropped (RIF)
	print("I2C7 CR1 = 0x", Hex{I2C7->CR1}, ", TIMINGR = 0x", Hex{I2C7->TIMINGR}, "\n");

	auto print_i2c_error = [&]() {
		// ErrorCode bits (stm32mp2xx_hal_i2c.h): 1=BERR 2=ARLO 4=AF(nack)
		// 8=OVR 0x10=DMA 0x20=TIMEOUT 0x40=SIZE
		print("  I2C ErrorCode 0x",
			  Hex{hi2c.ErrorCode},
			  ", ISR 0x",
			  Hex{I2C7->ISR},
			  ", SCL=",
			  int((GPIOD->IDR >> 15) & 1),
			  " SDA=",
			  int((GPIOD->IDR >> 14) & 1),
			  "\n");
	};

	auto pmic_read = [&](uint8_t reg, uint8_t &val) {
		return HAL_I2C_Mem_Read(&hi2c, PmicAddr, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
	};
	auto pmic_write = [&](uint8_t reg, uint8_t val) {
		return HAL_I2C_Mem_Write(&hi2c, PmicAddr, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
	};

	uint8_t product_id = 0, version = 0;
	if (auto res = pmic_read(ProductIdReg, product_id); res != HAL_OK) {
		print("ERROR: cannot reach PMIC on I2C7 (HAL_I2C_Mem_Read returned ", res, ")\n");
		print_i2c_error();

		print("Scanning the bus for any device that ACKs...\n");
		unsigned found = 0;
		for (uint16_t addr = 0x08; addr < 0x78; addr++) {
			if (HAL_I2C_IsDeviceReady(&hi2c, addr << 1, 1, 10) == HAL_OK) {
				print("  device ACKs at 7-bit address 0x", Hex{addr}, "\n");
				found++;
			}
		}
		if (!found)
			print("  none -- bus is dead (pins, kernel clock, or pad power)\n");
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
