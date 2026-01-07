#include "drivers/aarch64_system_reg.hh"
// #include "irq_ctrl.h"

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
}
