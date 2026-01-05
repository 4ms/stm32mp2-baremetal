#include "drivers/interrupt.hh"
#include "print.hh"
#include <cstdint>

int main()
{
	print("Interrupt test\n");

	while (true) {
	}
}

extern "C" void ISRHandler(unsigned irqnum)
{
	mdrivlib::Interrupt::callISR(irqnum);
}
