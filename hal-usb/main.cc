#include "interrupt.hh"
#include "print.hh"
#include "stm32mp2xx_hal.h"
#include "watchdog.hh"

int main()
{
	print("HAL USB Example\n");
	HAL_Init();

	PCD_HandleTypeDef pcd;
	HAL_PCD_Init(&pcd);

	while (true) {
		HAL_Delay(1000);
		print("Tick = ", HAL_GetTick(), "\n");
		watchdog_pet();
	}
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
