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
	constexpr uint8_t Buck3_900mV = 40;

	// I2C7 kernel clock (flexgen channel 15 = ck_ker_i2c7).
	// 64MHz / (findiv+1) = 64/4 = 16 MHz kernel clock -> ~100 kHz with the
	// TIMINGR below
	FlexbarConf i2c_xbar{.PLL = FlexbarConf::PLLx::HSI, .findiv = 3, .prediv = 0};
	i2c_xbar.init(15);

	__HAL_RCC_I2C7_FORCE_RESET();
	__HAL_RCC_I2C7_RELEASE_RESET();
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

	if (auto res = HAL_I2C_DeInit(&hi2c); res != HAL_OK) {
		print("ERROR: HAL_I2C_DeInit returned ", res, "\n");
		return false;
	}
	if (auto res = HAL_I2C_Init(&hi2c); res != HAL_OK) {
		print("ERROR: HAL_I2C_Init returned ", res, "\n");
		return false;
	}

	if (I2C7->CR1 == 0 && I2C7->TIMINGR == 0) {
		print("I2C7 registers were written but read back as 0, check RIF?\n");
	}

	auto print_i2c_error = [&]() {
		// ErrorCode bits (stm32mp2xx_hal_i2c.h): 1=BERR 2=ARLO 4=AF(nack)
		// 8=OVR 0x10=DMA 0x20=TIMEOUT 0x40=SIZE
		print("  I2C ErrorCode 0x", Hex{hi2c.ErrorCode});
		print(", ISR 0x", Hex{I2C7->ISR});
		print(", SCL=", int((GPIOD->IDR >> 15) & 1));
		print(", SDA=", int((GPIOD->IDR >> 14) & 1), "\n");
	};

	auto pmic_read = [&](uint8_t reg, uint8_t &val) {
		return HAL_I2C_Mem_Read(&hi2c, PmicAddr, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 1000);
	};
	auto pmic_write = [&](uint8_t reg, uint8_t val) {
		return HAL_I2C_Mem_Write(&hi2c, PmicAddr, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 1000);
	};

	uint8_t product_id = 0, version = 0;
	if (auto res = pmic_read(ProductIdReg, product_id); res != HAL_OK) {
		print("ERROR: cannot reach PMIC on I2C7 (HAL_I2C_Mem_Read returned ", res, ")\n");
		print_i2c_error();
		return false;
	}

	pmic_read(VersionReg, version);
	print("PMIC product ID 0x", Hex{product_id}, ", version 0x", Hex{version}, "\n");

	uint8_t cr1 = 0, cr2 = 0;
	pmic_read(Buck3MainCr1, cr1);
	pmic_read(Buck3MainCr2, cr2);
	print("Buck3 (VDDGPU) was: voltage code ", cr1, ", control 0x", Hex{cr2}, "\n");

	if (auto res = pmic_write(Buck3MainCr1, Buck3_900mV); res != HAL_OK) {
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
	print("Buck3 (VDDGPU) now: voltage code ", cr1, " (900mV), control 0x", Hex{cr2}, "\n");
	return true;
}
