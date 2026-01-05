#include "irq_ctrl.h"
void mmu_enable(void);

void system_init(void)
{
	mmu_enable();
	IRQ_Initialize();
}
