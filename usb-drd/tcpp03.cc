/*
 * tcpp03.c — TCPP03-M20 USB Type-C port protection controller
 *
 * Stub implementation — fill in GPIO and I2C details for the EV1 board.
 */

#include "tcpp03.hh"
#include "drivers/pin.hh"
#include "drivers/rcc_xbar.hh"
#include <cstdio>
#include <cstring>
#include <log.h>
#include <optional>

int Tcpp03Controller::init()
{
	if (auto res = i2c_init(); res != 0)
		return res;

	printf("TCPP03: init\n");

	return 0;
}

int Tcpp03Controller::enable_vbus()
{
	printf("TCPP03: enable VBUS\n");

	// TCPP03-M20 needs to be enabled before it will respond to I2C:
	Pin enable{GPIO::G, PinNum::_2, PinMode::Output};
	enable.on();

	HAL_Delay(10);
	uint8_t data = 0;

	data = (0b01 << 4); // Power Mode: Normal
	write_reg(register0, data);
	HAL_Delay(2);

	data = (0b01 << 4); // Power Mode: Normal
	data |= (1 << 3);	// Open GD Consumer
	data |= (1 << 2);	// Close GD Producer
	write_reg(register0, data);
	HAL_Delay(2);

	// Read back to acknowledge
	data = read_reg(register1).value_or(0xFF);
	if (data != 0x1C)
		pr_err("TCPP03: Error, ACK register should be 0x1c, but is 0x%02x\n", data);

	return 0;
}

int Tcpp03Controller::disable_vbus()
{
	printf("TCPP03: disable VBUS\n");

	// Power Mode: Normal operation (not hibernating or low-power)
	// Gate Driver Provider switch open => no 5V routed to USB jack VBUS
	// Gate Driver consumer closed
	uint8_t data = (0b01 << 4);

	write_reg(register0, data);
	data = read_reg(register1).value_or(0xFF);
	if (data != 0x10) {
		pr_err("TCPP03: Error, ACK register should be 0x10, but is 0x%02x\n", data);
		return -1;
	}

	return 0;
}

int Tcpp03Controller::i2c_init()
{
	printf("Setting I2C1 XBAR\n");
	FlexbarConf i2c_xbar{.PLL = FlexbarConf::PLLx::_4, .findiv = 0x10, .prediv = 0};
	i2c_xbar.init(12);

	printf("Enable I2C1 clock\n");
	__HAL_RCC_I2C1_CLK_ENABLE();

	printf("Init I2C1 pins\n");
	Pin scl{GPIO::G,
			PinNum::_13,
			PinMode::Alt,
			AltFunc9,
			PinPull::None,
			PinPolarity::Normal,
			PinSpeed::Medium,
			PinOType::OpenDrain};

	Pin sda{GPIO::I,
			PinNum::_1,
			PinMode::Alt,
			AltFunc9,
			PinPull::None,
			PinPolarity::Normal,
			PinSpeed::Medium,
			PinOType::OpenDrain};

	printf("Init I2C1 periph\n");
	memset(&hi2c, 0, sizeof hi2c);
	hi2c.Instance = I2C1;
	hi2c.Init.Timing = 0x10702525; // about 100kHz
	hi2c.Init.OwnAddress1 = 0x33;
	hi2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c.Init.OwnAddress2 = 0;
	hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	hi2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;

	if (auto res = HAL_I2C_DeInit(&hi2c); res != HAL_OK) {
		printf("HAL_I2C_DeInit failed: %d\n", res);
		return res;
	}

	if (auto res = HAL_I2C_Init(&hi2c); res != HAL_OK) {
		printf("ERROR: HAL_I2C_Init returned: %d\n", res);
		return res;
	}

	if (auto res = HAL_I2CEx_ConfigAnalogFilter(&hi2c, I2C_ANALOGFILTER_ENABLE); res != HAL_OK) {
		printf("ERROR: HAL_I2CEx_ConfigAnalogFilter returned: %d\n", res);
		return res;
	}

	if (auto res = HAL_I2CEx_ConfigDigitalFilter(&hi2c, 8); res != HAL_OK) {
		printf("ERROR: HAL_I2CEx_ConfigDigitalFilter returned: %d\n", res);
		return res;
	}
	return 0;
}
int Tcpp03Controller::write_reg(uint8_t reg, uint8_t data)
{
	printf("TCPP03: Writing 0x%02x to register 0x%02x\n", data, reg);
	if (auto res = HAL_I2C_Mem_Write(&hi2c, dev_address, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 0x10000); res != HAL_OK) {
		printf("ERROR: HAL_I2C_Mem_Write returned %d\n", res);
		return res;
	}
	return 0;
}

std::optional<uint8_t> Tcpp03Controller::read_reg(uint8_t reg)
{
	uint8_t data = 0;

	if (auto res = HAL_I2C_Mem_Read(&hi2c, dev_address, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 0x10000); res != HAL_OK) {
		printf("ERROR: HAL_I2C_Mem_Read returned %d\n", res);
		return std::nullopt;
	} else {
		printf("TCPP03: Reading register 0x%02x = 0x%02x\n", reg, data);
		return data;
	}
}
