#include "interrupt.hh"
#include "stm32mp2xx_hal.h"
#include "usb_serial_device.hh"
#include "usbd_def.h"
#include "watchdog.hh"
#include <cstdio>

// global so can be accessed by ST's HAL
PCD_HandleTypeDef hpcd;
USBD_HandleTypeDef pDevice;

int main()
{
	printf("HAL USB Example\n");
	HAL_Init();

	UsbSerialDevice serial{&pDevice};

	InterruptManager::register_and_start_isr(USB3DR_IRQn, 0, 0, [] { HAL_PCD_IRQHandler(&hpcd); });

	//??
	InterruptManager::register_and_start_isr(USB3DR_BC_IRQn, 0, 0, [] { printf("BC\n"); });

	printf("Starting UsbSerialDevice...\n");
	serial.start();
	printf("Started UsbSerialDevice\n");

	while (true) {
		HAL_Delay(1000);
		printf("Tick = %u\n", HAL_GetTick());
		watchdog_pet();
	}
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	printf("assert failed: %s:%u\n", file, line);
}
