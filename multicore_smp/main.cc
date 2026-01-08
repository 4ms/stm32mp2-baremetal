#include "interrupt.hh"
#include "print.hh"
#include "psci.hh"
#include <cstdint>

void delay(unsigned x)
{
	volatile unsigned xx = x;
	while (xx--)
		;
}

// Defined in assembly:
extern "C" void aux_core_startup(void);

extern "C" void aux_main()
{
	print("Aux is online\n");

	InterruptManager::register_and_start_isr(SGI1_IRQn, 0, 0, []() { print("> Core1 SGI received\n"); });

	int x = 500'000;
	while (true) {
		x++;
		if (x % 1'000'000 == 0) {
			// print("T1ck\n");
			print("Core1 sending SGI to Core0...\n");
			GIC_SendSGI(SGI1_IRQn, 0b01, 0b00);
		}
	}
}

int main()
{
	print("Multicore A35 Test\n");

	auto ret = start_cpu1(aux_core_startup, 0);

	delay(1000000);

	print("System call to start aux CA35 core returned ", ret, " (0=success)\n");

	InterruptManager::register_and_start_isr(SGI1_IRQn, 0, 0, []() {
		delay(500000);
		print("> Core0 SGI received (delayed)\n");
	});

	delay(1000000);

	int x;
	while (true) {
		x++;
		if (x % 40'000'000 == 0) {
			print("Core0 sending SGI to both cores...\n");
			GIC_SendSGI(SGI1_IRQn, 0b11, 0b00);
		} else if (x % 10'000'000 == 0) {
			// print("T0ck\n");
			print("Core0 sending SGI to Core1...\n");
			GIC_SendSGI(SGI1_IRQn, 0b10, 0b00);
		}
	}
}
