#include "drivers/pin.hh"
#include "drivers/rcc_xbar.hh"
#include "interrupt.hh"
#include "print.hh"
#include "stm32mp2xx_hal.h"
#include "stm32mp2xx_hal_def.h"
#include "watchdog.hh"
#include <cmath>

static_assert(I2C2_BASE == 0x40130000);
// RIF resource ID 42
// APB1
// ck_ker_i2c is flexgen channel 12 (max 200MHz)

static void i2c_mem_tx_complete_cb(I2C_HandleTypeDef *hi2c);
static void i2c_mem_rx_complete_cb(I2C_HandleTypeDef *hi2c);
static void i2c_error_cb(I2C_HandleTypeDef *hi2c);

int main()
{
	print("I2C Example\n");
	HAL_Init();

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
	I2C_HandleTypeDef hi2c;
	hi2c.Instance = I2C2;
	hi2c.Init.Timing = 0x507075B1;
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

	uint8_t data[4]{0, 0, 0, 0};

	InterruptManager::register_and_start_isr(I2C2_IRQn, 1, 1, [&hi2c, &data]() {
		HAL_I2C_EV_IRQHandler(&hi2c);
		HAL_I2C_ER_IRQHandler(&hi2c);
		print("I2C IRQ\n");
	});
	HAL_I2C_RegisterCallback(&hi2c, HAL_I2C_MEM_TX_COMPLETE_CB_ID, i2c_mem_tx_complete_cb);
	HAL_I2C_RegisterCallback(&hi2c, HAL_I2C_MEM_RX_COMPLETE_CB_ID, i2c_mem_rx_complete_cb);
	HAL_I2C_RegisterCallback(&hi2c, HAL_I2C_ERROR_CB_ID, i2c_error_cb);

	// Un-shifted address: 0x46
	// Shifted: 0x8C for write, 0x8D for read
	constexpr uint8_t dev_address = 0b1000110;
	constexpr uint8_t read_mem_address = 70;  // default value is 0b1101'0111 = 0xD7
	constexpr uint8_t write_mem_address = 65; // can write and read 8 bits

	print("Reading memory, blocking\n");
	if (auto res = HAL_I2C_Mem_Read(&hi2c, dev_address, read_mem_address, I2C_MEMADD_SIZE_8BIT, data, 1, 0x10000);
		res != HAL_OK)
	{
		print("ERROR: HAL_I2C_Mem_Read returned ", res, "\n");
	} else {
		print("Read 0x", Hex{data[0]}, " from register 0x", Hex{read_mem_address}, "\n");
	}

	data[0] = 0x55;

	print("Writing 0x", Hex{data[0]}, " to register 0x", Hex{write_mem_address}, " in blocking mode\n");
	if (auto res = HAL_I2C_Mem_Write(&hi2c, dev_address, write_mem_address, I2C_MEMADD_SIZE_8BIT, data, 1, 0x10000);
		res != HAL_OK)
	{
		print("ERROR: HAL_I2C_Mem_Write returned ", res, "\n");
	}

	data[0] = 0x00;

	print("Reading memory via interrupt\n");
	if (auto res = HAL_I2C_Mem_Read_IT(&hi2c, dev_address, write_mem_address, I2C_MEMADD_SIZE_8BIT, data, 1);
		res != HAL_OK)
	{
		print("ERROR: HAL_I2C_Mem_Read_IT returned ", res, "\n");
	} else {
		uint32_t timeout_tm = HAL_GetTick() + 5000;
		while (HAL_I2C_GetState(&hi2c) != HAL_I2C_STATE_READY) {
			if (HAL_GetTick() >= timeout_tm) {
				print("ERROR: Timed out waiting for Mem Read interrupt\n");
				break;
			}
		}
		print("Read 0x", Hex{data[0]}, " from register 0x", Hex{write_mem_address}, "\n");
	}

	// Endless loop
	volatile int x = 0x10000000;
	while (true) {
		x = x + 1;
		if ((x % 10'000'000) == 0) {
			print("Tick = ", HAL_GetTick(), "\n");
			watchdog_pet();
		}
	}
}
void i2c_mem_tx_complete_cb(I2C_HandleTypeDef *hi2c)
{
	print("I2C mem tx callback\n");
}

void i2c_mem_rx_complete_cb(I2C_HandleTypeDef *hi2c)
{
	print("I2C mem rx callback\n");
}

void i2c_error_cb(I2C_HandleTypeDef *hi2c)
{
	print("I2C error callback\n");
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
