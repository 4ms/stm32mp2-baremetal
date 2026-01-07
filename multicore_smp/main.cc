#include "print.hh"
#include <cstdint>

extern "C" void aux_main()
{
	print("Aux is online\n");

	int x = 500'000;
	while (true) {
		x++;
		if (x % 1'000'000 == 0)
			print("Core1\n");
	}
}

int start_cpu1(void (*cpu1_entry)(void), uint64_t context);

extern "C" void aux_core_startup(void);

int main()
{
	print("Multicore A35 Test\n");

	auto ret = start_cpu1(aux_core_startup, 0);

	volatile uint64_t x = 1000000;
	while (x--) {
	}

	print("Started, returned ", ret, "\n");

	while (true) {
		x++;
		if (x % 10'000'000 == 0)
			print("Core0\n");
	}
}
