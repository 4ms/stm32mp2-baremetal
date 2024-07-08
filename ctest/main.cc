#include <cstdint>

void delay(unsigned);
void set_bss_char(char x);

const char ro_char = 'X';
char data_char = 'Y';
char bss_char;

int main()
{
	volatile uint32_t *uart = reinterpret_cast<volatile uint32_t *>(0x400E0028);

	set_bss_char('Z');

	while (true) {
		*uart = ro_char;
		*uart = data_char;
		*uart = bss_char;
		delay(0xFFFFFFFF);
	}
}

void delay(unsigned x)
{
	while (x--)
		;
}

void set_bss_char(char x)
{
	bss_char = x;
}
