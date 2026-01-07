#include "interrupt.hh"
#include "print.hh"

int main()
{
	print("HAL Example\n");

	InterruptManager::register_and_start_isr(SGI1_IRQn, 0, 0, []() {});

	while (true) {
		asm("nop");
	}
}
