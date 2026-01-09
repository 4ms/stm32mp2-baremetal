#include "interrupt.hh"
#include "print.hh"
#include "psci.hh"
#include "watchdog.hh"

void delay(unsigned x)
{
	volatile unsigned xx = x;
	while (xx--)
		;
}

// Defined in assembly:
extern "C" void aux_core_startup();

extern "C" void aux_main();

int main()
{
	print("Multicore A35 Test\n");

	auto ret = start_cpu1(aux_core_startup, 0);

	delay(1000000);

	print("System call to start aux CA35 core returned ", ret, " (0=success)\n");

	IRQ_Initialize();

	// TODO: we can't yet have an interrupt enabled on different cores with different handlers
	// So we use SGI1 for Core1 and SGI6 for Core0
	InterruptManager::register_and_start_isr(SGI6_IRQn, 1, 1, []() { print("> Core0 SGI6 received\n"); });

	int x;
	while (true) {
		x++;
		if (x % 20'000'000 == 0) {
			print("T0ck\n");
			// GIC_SendSGI(SGI6_IRQn, 0b01, 0b00); // this triggers our own IRQ handler
			GIC_SendSGI(SGI1_IRQn, 0b10, 0b00);
		}
	}
}

extern "C" void aux_main()
{
	print("Aux is online\n");

	InterruptManager::register_and_start_isr(SGI1_IRQn, 2, 0, []() { print("> Core1 SGI1 received\n"); });

	print("Aux enabled SGI\n");

	int x = 10'000'000; // stagger Core1 and Core0 print()
	while (true) {
		x++;
		if (x % 20'000'000 == 0) {
			print("T1ck\n");
			// FixMe: Why does this not trigger SGI6 on core0? It works when called from Core0
			GIC_SendSGI(SGI6_IRQn, 0b01, 0b00);

			watchdog_pet();
		}
	}
}
