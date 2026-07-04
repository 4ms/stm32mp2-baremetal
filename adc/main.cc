#include "drivers/watchdog.hh"
#include "print/print.hh"

static int check_data();

int main()
{
	print("ADC Example\n");

	while (true) {
		asm("nop");
	}
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
