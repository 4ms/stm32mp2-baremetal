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

	int x = 10'000'000; // stagger Core1 and Core0 print()
	while (true) {
		x++;
		if (x % 20'000'000 == 0)
			print("T1ck\n");
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
		if (x % 20'000'000 == 0)
			print("T0ck\n");
	}
}
