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

asm(".global secondary_entry\n"
	"secondary_entry:\n"
	"    ldr  x0, =_cpu1_stack_start\n"
	"    mov  sp, x0\n"
	"    dsb  sy\n"
	"    isb\n"
	"    ic   iallu\n"
	"    dsb  sy\n"
	"    isb\n"
	"    bl   aux_main\n"
	"1:  wfe\n"
	"    b    1b\n");

int start_cpu1(void (*cpu1_entry)(void), uint64_t context);
extern "C" void secondary_entry(void);

int main()
{
	print("HAL Test\n");

	auto ret = start_cpu1(secondary_entry, 0);

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
