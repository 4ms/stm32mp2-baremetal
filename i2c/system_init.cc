#include "drivers/aarch64_system_reg.hh"

extern "C" {
uint32_t SystemCoreClock;
}

extern "C" void mmu_enable(void);
extern "C" void IRQ_Initialize(void);

extern "C" void system_init(void)
{
	mmu_enable();

	enable_fiq();
	enable_irq();
	enable_debug_exceptions();
	enable_serror_exceptions();

	IRQ_Initialize();

	SystemCoreClock = 1'200'000'000; // exact value is not important for these example projects
}
