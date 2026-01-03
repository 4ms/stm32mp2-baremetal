#include <cstdint>

extern "C" void early_puts(const char *s);
extern "C" void early_puthex64(uint64_t v);

int main()
{
	early_puts("Interrupt test\n");

	while (true) {
	}
}

extern "C" void ISRHandler()
{}
