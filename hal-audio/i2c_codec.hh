#pragma once
#include "bitfield.hh"
#include "codec_PCM3168_registers.hh"
#include "drivers/pin.hh"
#include "drivers/rcc_xbar.hh"
#include "interrupt.hh"
#include "print.hh"
#include "stm32mp2xx_hal.h"
#include <cstdint>
#include <cstring>

using namespace CodecPCM3168;

struct CodecI2C {
	uint8_t data[4]{0, 0, 0, 0};
	I2C_HandleTypeDef hi2c;
	uint8_t dev_addr;
	constexpr static uint8_t PCM3168_ADDR = 0b10001010;

	CodecI2C()
		: dev_addr{PCM3168_ADDR}
	{
		print("Setting I2C2 XBAR\n");
		FlexbarConf i2c_xbar{.PLL = FlexbarConf::PLLx::_4, .findiv = 0x30, .prediv = 0};
		i2c_xbar.init(12);

		print("PLL4 freq = ", HAL_RCCEx_GetPLL4ClockFreq(), "\n");
		uint32_t freq = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_I2C1_2);
		print("I2C2 kernel clock freq = ", freq, "\n");

		print("Enable I2C2 clock\n");
		__HAL_RCC_I2C2_CLK_ENABLE();
		__HAL_RCC_I2C2_FORCE_RESET();
		__HAL_RCC_I2C2_RELEASE_RESET();

		print("Init I2C2 pins\n");
		// SCL: header pin 28
		Pin scl{GPIO::B,
				PinNum::_5,
				PinMode::Alt,
				AltFunc9,
				PinPull::None,
				PinPolarity::Normal,
				PinSpeed::Medium,
				PinOType::OpenDrain};
		// SDA: header pin 27
		Pin sda{GPIO::B,
				PinNum::_4,
				PinMode::Alt,
				AltFunc9,
				PinPull::None,
				PinPolarity::Normal,
				PinSpeed::Medium,
				PinOType::OpenDrain};

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
	}

	bool init_codec()
	{
		// Reset pin low->high

		Pin codec_reset{GPIO::B, PinNum::_12, PinMode::Output}; // pin 37
		codec_reset.off();
		HAL_Delay(100);
		codec_reset.on();
		HAL_Delay(1);

		for (auto packet : stereo_codec_init) {
			if (!write_register(packet.reg_num, packet.value))
				return false;
		}
		return true;
	}

	uint8_t read_register(uint8_t reg_addr)
	{
		print("Reading I2C device 0x", Hex{dev_addr}, " register 0x", Hex{reg_addr}, " in blocking mode\n");
		if (auto res = HAL_I2C_Mem_Read(&hi2c, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, data, 1, 0x10000);
			res != HAL_OK)
		{
			print("ERROR: HAL_I2C_Mem_Read returned ", res, "\n");
		} else {
			print("Read 0x", Hex{data[0]}, "\n");
		}
		return data[0];
	}

	bool write_register(uint8_t reg_addr, uint8_t value)
	{
		data[0] = value;

		print("Writing 0x", Hex{data[0]}, " to device 0x", Hex{dev_addr}, " register 0x", Hex{reg_addr}, "\n");

		if (auto res = HAL_I2C_Mem_Write(&hi2c, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, data, 1, 0x10000);
			res != HAL_OK)
		{
			print("ERROR: HAL_I2C_Mem_Write returned ", res, "\n");
			return false;
		}

		return true;
	}

private:
	constexpr static RegisterData stereo_codec_init[] = {
		{ResetControl::Address, bitfield(ResetControl::NoReset, ResetControl::NoResync, ResetControl::Single)}, // 41 c1
		{DacControl1::Address, bitfield(DacControl1::I2S_24bit, DacControl1::SlaveMode)},						// 42 00
		{DacControl2::Address,
		 bitfield(DacControl2::Dac12Enable,
				  DacControl2::Dac34Disable,
				  DacControl2::Dac56Disable,
				  DacControl2::Dac78Disable)},					   // 43 e0
		{DacSoftMute::Address, bitfield(DacSoftMute::NoDacMuted)}, // 44 00
		{DacAllAtten::Address, bitfield(DacAtten::ZeroDB)},
		{AdcSamplingMode::Address, bitfield(AdcSamplingMode::Single)},
		{AdcControl1::Address, bitfield(AdcControl1::SlaveMode, AdcControl1::I2S_24bit)},
		{AdcControl2::Address, bitfield(AdcControl2::AdcAllHPFDisabled)},
		{AdcSoftMute::Address,
		 bitfield(AdcSoftMute::Adc3Mute, AdcSoftMute::Adc4Mute, AdcSoftMute::Adc5Mute, AdcSoftMute::Adc6Mute)},
		{AdcAllAtten::Address, bitfield(AdcAtten::ZeroDB)},
		{AdcInputType::Address, bitfield(AdcInputType::AllAdcDifferential)},

	};
};
