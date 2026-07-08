// Cortex-M33 coprocessor firmware for the STM32MP257F.
//
// Runs in parallel with the A35 after the A35 loads this image into SRAM2 and
// releases CPU2 from hold-boot. Prints occasionally over the shared console
// UART and toggles PF9.
//
// The M33 runs *secure* (the A35 sets CFG_SECEXT before releasing it), with
// the SAU left disabled -- so the whole address map is Secure and every bus
// access the M33 makes is a secure transaction. That means:
//  - it can use the same peripheral addresses the A35 uses (0x4xxxxxxx),
//    no need for the 0x5xxxxxxx secure aliases;
//  - it can drive GPIO pins directly, since pins reset to secure
//    (GPIOx_SECCFGR = 0xFFFF) and secure accesses are always allowed.

#include "drivers/pin.hh"
#include "print/print.hh"
#include <cstdint>

extern "C" void init_uart(void);

namespace
{
void delay(unsigned n)
{
	for (unsigned i = 0; i < n; i++)
		asm("nop");
}
} // namespace

int main()
{
	// The A35 has already brought up the console UART, but calling init_uart()
	// again is safe and makes the M33 independent of A35 timing.
	init_uart();

	print("\nM33: * yawn *\nM33: Hello from Cortex-M33!\n");
	print("M33: I will toggle PF9 and print occasionally.\n");

	Pin pf9{GPIO::F, PinNum::_9, PinMode::Output};

	unsigned count = 0;
	while (true) {
		pf9.on();
		delay(2'000'000);
		pf9.off();
		delay(2'000'000);

		if ((++count % 4) == 0)
			print("M33: tick ", (int)count, "\n");
	}
}
