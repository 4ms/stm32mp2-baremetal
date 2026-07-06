/*
 * tcpp03.h — TCPP03-M20 USB Type-C port protection controller
 *
 * Controls VBUS source power via GPIO enable + I2C configuration
 * on the STM32MP257 EV1 board.
 */

#pragma once
#include "stm32mp2xx.h"
#include <cstdint>
#include <optional>

struct Tcpp03Controller {

	int init();

	int enable_vbus();

	int disable_vbus();

private:
	int i2c_init();
	int write_reg(uint8_t reg, uint8_t val);
	std::optional<uint8_t> read_reg(uint8_t reg);

	I2C_HandleTypeDef hi2c;

	// TCPP03-M20 address: 0x34 unshifted, 0x68/69 shifted
	static constexpr uint8_t dev_address = 0b01101000;
	static constexpr uint8_t register0 = 0;
	static constexpr uint8_t register1 = 1;
	static constexpr uint8_t register2 = 2;
};
