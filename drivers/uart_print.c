#include <stdint.h>

#define USART2_BASE 0x400E0000UL

typedef struct {
	volatile uint32_t CR1;	// 0x00
	volatile uint32_t CR2;	// 0x04
	volatile uint32_t CR3;	// 0x08
	volatile uint32_t BRR;	// 0x0C
	volatile uint32_t GTPR; // 0x10
	volatile uint32_t RTOR; // 0x14
	volatile uint32_t RQR;	// 0x18
	volatile uint32_t ISR;	// 0x1C
	volatile uint32_t ICR;	// 0x20
	volatile uint32_t RDR;	// 0x24
	volatile uint32_t TDR;	// 0x28
} stm32_usart_t;

#define USART2 ((stm32_usart_t *)USART2_BASE)

void putchar_s(char c)
{
	// Wait until transmit data register empty (TXE = bit 7)
	while (!(USART2->ISR & (1U << 7)))
		;

	USART2->TDR = (uint32_t)c;
}

void early_puts(const char *s)
{
	while (*s) {
		if (*s == '\n')
			putchar_s('\r');
		putchar_s(*s++);
	}
}

void early_puthex64(uint64_t v)
{
	static const char hex[] = "0123456789abcdef";
	for (int i = 60; i >= 0; i -= 4)
		putchar_s(hex[(v >> i) & 0xF]);
}

void early_print_el(void)
{
	unsigned el;
	__asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
	early_puts("CurrentEL=");
	early_puthex64((el >> 2) & 3);
	early_puts("\n");
}
