#include <cstdint>

void delay(unsigned);
void set_bss_char(char x);

// Test globals:

// Initialized constant:
const char ro_char = 'X';

// Initialized non-constant:
char data_char = 'Y';

// Uninitialized:
char bss_char;

int main()
{
	// volatile uint32_t *uart = reinterpret_cast<volatile uint32_t *>(0x400E0028);
	volatile uint32_t *uart = reinterpret_cast<volatile uint32_t *>(0x40220028);

	if (bss_char == 0)
		set_bss_char('Z');
	else
		set_bss_char('*');

	delay(1);

	while (true) {
		*uart = ro_char;
		delay(1);
		*uart = data_char;
		delay(1);
		*uart = bss_char;
		delay(1);
		delay(0xFFFFFFFF);
	}
}

void delay(unsigned x)
{
	volatile unsigned y = x;
	while (true) {
		y = y - 1;
		if (y == 0)
			break;
	}
}

void set_bss_char(char x)
{
	bss_char = x;
}
