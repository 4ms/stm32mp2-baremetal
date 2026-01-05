#include "interrupt.hh"
#include "print.hh"

void delay(unsigned x)
{
	volatile unsigned xx = 0;
	while (xx < x) {
		xx++;
	}
}

int main()
{
	print("Nested Interrupts with priorities test\n");

	InterruptManager::register_and_start_isr(SGI1_IRQn, 0, 0, []() {
		print(" > SGI1 handler started\n");
		print(" < SGI1 handler ended\n");
	});

	InterruptManager::register_and_start_isr(SGI2_IRQn, 2, 3, []() {
		print("> SGI2 handler started\n");
		print("  SGI2: Sending SGI1\n");
		GIC_SendSGI(SGI1_IRQn, 0b01, 0b00);
		print("< SGI2 handler ended\n");
	});

	InterruptManager::register_and_start_isr(SGI3_IRQn, 3, 0, []() {
		print("> SGI3 handler started\n");
		print("< SGI3 handler ended\n");
	});

	InterruptManager::register_and_start_isr(USART2_IRQn, 3, 1, []() {
		print("> USART2 handler started\nReceived:\n   > ");

		while (USART2->ISR & USART_ISR_RXFNE) {
			char cc[2];
			cc[0] = USART2->RDR;
			cc[1] = 0;
			print(cc);
		}
		print("\nNow sending SGI2: Expect SGI now:\n");
		GIC_SendSGI(SGI2_IRQn, 0b01, 0b00);

		print("Now sending SGI3... (should happen after USART2 exits)\n");
		GIC_SendSGI(SGI3_IRQn, 0b01, 0b00);

		print("< USART2 handler ended: Expect SGI3 now:\n");
	});

	USART2->CR1 &= ~USART_CR1_UE;
	USART2->CR1 |= USART_CR1_RE; // enable RX
	USART2->CR3 |= USART_CR3_RXFTIE;
	USART2->CR1 |= USART_CR1_UE;
	USART2->CR1 |= USART_CR1_RXFNEIE;

	print("\nPress a key to trigger USART2 RX IRQ\n");

	while (true) {
		asm("nop");
	}
}
