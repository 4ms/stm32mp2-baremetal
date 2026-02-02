#pragma once
#include "drivers/pin.hh"
#include "drivers/rcc_xbar.hh"
#include "interrupt.hh"
#include "print.hh"
#include "stm32mp2xx_hal.h"
#include <cstdint>
#include <cstring>

struct CodecI2C2 {
	uint8_t data[4]{0, 0, 0, 0};
	I2C_HandleTypeDef hi2c;

	CodecI2C2()
	{
		print("Setting I2C2 XBAR\n");
		FlexbarConf i2c2_xbar{.PLL = FlexbarConf::PLLx::_4, .findiv = 0x3F, .prediv = 0};
		i2c2_xbar.init(12);

		print("PLL4 freq = ", HAL_RCCEx_GetPLL4ClockFreq(), "\n");
		uint32_t freq = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_I2C1_2);
		print("I2C2 kernel clock freq = ", freq, "\n");

		print("Enable I2C2 clock\n");
		__HAL_RCC_I2C2_CLK_ENABLE();
		__HAL_RCC_I2C2_FORCE_RESET();
		__HAL_RCC_I2C2_RELEASE_RESET();

		print("Init I2C2 pins\n");
		Pin scl{GPIO::B, PinNum::_5, PinMode::Alt, AltFunc9, PinPull::Up}; // header pin 28
		Pin sda{GPIO::B, PinNum::_4, PinMode::Alt, AltFunc9, PinPull::Up}; // header pin 27

		print("Init I2C2 periph\n");
		std::memset(&hi2c, 0, sizeof hi2c);
		hi2c.Instance = I2C2;
		hi2c.Init.Timing = 0x10702525; // about 100kHz
		hi2c.Init.OwnAddress1 = 0x33;
		hi2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
		hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
		hi2c.Init.OwnAddress2 = 0;
		hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
		hi2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
		hi2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;

		if (auto res = HAL_I2C_DeInit(&hi2c); res != HAL_OK) {
			print("HAL_I2C_DeInit failed ", res, "\n");
		}

		if (auto res = HAL_I2C_Init(&hi2c); res != HAL_OK) {
			print("ERROR: HAL_I2C_Init returned ", res, "\n");
		}

		if (auto res = HAL_I2CEx_ConfigAnalogFilter(&hi2c, I2C_ANALOGFILTER_ENABLE); res != HAL_OK) {
			print("ERROR: HAL_I2CEx_ConfigAnalogFilter returned ", res, "\n");
		}

		if (auto res = HAL_I2CEx_ConfigDigitalFilter(&hi2c, 8); res != HAL_OK) {
			print("ERROR: HAL_I2CEx_ConfigDigitalFilter returned ", res, "\n");
		}

		InterruptManager::register_and_start_isr(I2C2_IRQn, 1, 1, [this]() {
			HAL_I2C_EV_IRQHandler(&hi2c);
			HAL_I2C_ER_IRQHandler(&hi2c);
			print("I2C IRQ\n");
		});

		HAL_I2C_RegisterCallback(&hi2c, HAL_I2C_MEM_TX_COMPLETE_CB_ID, i2c_mem_tx_complete_cb);
		HAL_I2C_RegisterCallback(&hi2c, HAL_I2C_MEM_RX_COMPLETE_CB_ID, i2c_mem_rx_complete_cb);
		HAL_I2C_RegisterCallback(&hi2c, HAL_I2C_ERROR_CB_ID, i2c_error_cb);
	}

	uint8_t read_register(uint8_t dev_addr, uint8_t reg_addr)
	{
		print("Reading I2C device register, blocking\n");
		if (auto res = HAL_I2C_Mem_Read(&hi2c, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, data, 1, 0x10000);
			res != HAL_OK)
		{
			print("ERROR: HAL_I2C_Mem_Read returned ", res, "\n");
		} else {
			print("Read 0x", Hex{data[0]}, " from register 0x", Hex{dev_addr}, "\n");
		}
		return data[0];
	}

	void write_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t value)
	{
		data[0] = value;
		print("Writing 0x", Hex{data[0]}, " to register 0x", Hex{reg_addr}, " in blocking mode\n");
		if (auto res = HAL_I2C_Mem_Write(&hi2c, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, data, 1, 0x10000);
			res != HAL_OK)
		{
			print("ERROR: HAL_I2C_Mem_Write returned ", res, "\n");
		}
	}

	uint8_t read_register_interrupt(uint8_t dev_addr, uint8_t reg_addr)
	{
		print("Reading I2C device memory via interrupt\n");
		if (auto res = HAL_I2C_Mem_Read_IT(&hi2c, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, data, 1); res != HAL_OK) {
			print("ERROR: HAL_I2C_Mem_Read_IT returned ", res, "\n");
		} else {
			uint32_t timeout_tm = HAL_GetTick() + 2000;
			while (HAL_I2C_GetState(&hi2c) != HAL_I2C_STATE_READY) {
				if (HAL_GetTick() >= timeout_tm) {
					print("ERROR: Timed out waiting for Mem Read interrupt\n");
					break;
				}
			}
			print("Read 0x", Hex{data[0]}, " from register 0x", Hex{reg_addr}, "\n");
		}
		return data[0];
	}

	void write_register_interrupt(uint8_t dev_addr, uint8_t reg_addr, uint8_t value)
	{
		print("Writing I2C device memory via interrupt\n");
		data[0] = value;
		if (auto res = HAL_I2C_Mem_Write_IT(&hi2c, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, data, 1); res != HAL_OK) {
			print("ERROR: HAL_I2C_Mem_Write_IT returned ", res, "\n");
		} else {
			uint32_t timeout_tm = HAL_GetTick() + 2000;
			while (HAL_I2C_GetState(&hi2c) != HAL_I2C_STATE_READY) {
				if (HAL_GetTick() >= timeout_tm) {
					print("ERROR: Timed out waiting for Mem Write interrupt\n");
					break;
				}
			}
		}
	}

	static void i2c_mem_tx_complete_cb(I2C_HandleTypeDef *hi2c)
	{
		print("I2C mem tx callback\n");
	}

	static void i2c_mem_rx_complete_cb(I2C_HandleTypeDef *hi2c)
	{
		print("I2C mem rx callback\n");
	}

	static void i2c_error_cb(I2C_HandleTypeDef *hi2c)
	{
		print("I2C error callback\n");
	}
};
