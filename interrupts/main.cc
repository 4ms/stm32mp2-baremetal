#include "drivers/interrupt.hh"
#include "print.hh"

int main()
{
	print("Interrupt test\n");

	IRQ_Initialize();

	mdrivlib::InterruptManager::register_and_start_isr(SGI1_IRQn, 0, 0, []() {
		print("SGI1 handler started\n");
		print("SGI1 handler ended\n");
	});

	mdrivlib::InterruptManager::register_and_start_isr(SGI2_IRQn, 0, 0, []() {
		print("SGI2 handler started\n");
		print("SGI2 handler ended\n");
	});

	mdrivlib::InterruptManager::register_and_start_isr(SGI3_IRQn, 0, 0, []() {
		print("SGI3 handler started\n");
		print("SGI3 handler ended\n");
	});

	// GIC_SendSGI

	while (true) {
		asm("nop");
	}
}

extern "C" void ISRHandler(unsigned irqnum)
{
	mdrivlib::Interrupt::callISR(irqnum);
}
