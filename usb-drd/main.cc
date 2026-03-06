#include "drivers/pin.hh"
#include "drivers/rcc.hh"
#include "drivers/rcc_xbar.hh"
#include "interrupt/interrupt.hh"
#include "print/print.hh"
#include "stm32mp2xx_hal.h"
#include <cmath>

int main()
{
	print("USB DRD Example\n");
	HAL_Init();

	while (true) {
		asm("nop");
	}
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
