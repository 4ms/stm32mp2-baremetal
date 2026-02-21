#include "drivers/button.hh"
#include "interrupt/interrupt.hh"
#include "print/print.hh"

#if UART == 6
#define USARTx USART6
#define USARTx_IRQn USART6_IRQn
#else
#define USARTx USART2
#define USARTx_IRQn USART6_IRQn
#endif

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

	InterruptManager::register_and_start_isr(SGI4_IRQn, 3, 1, []() {
		print("> User2 button pressed -> SGI4 started\n");

		print("\nSending SGI2: Expect SGI2 now:\n");
		GIC_SendSGI(SGI2_IRQn, 0b01, 0b00);

		print("Sending SGI3... (should happen after SGI4 exits)\n");
		GIC_SendSGI(SGI3_IRQn, 0b01, 0b00);

		print("< SGI4 handler ended: Expect SGI3 now:\n");
	});

	InterruptManager::register_and_start_isr(USARTx_IRQn, 3, 1, []() {
		print("> USARTx handler started\nReceived:\n   > ");

		while (USARTx->ISR & USART_ISR_RXFNE) {
			char cc[2];
			cc[0] = USARTx->RDR;
			cc[1] = 0;
			print(cc);
		}
		print("\nNow sending SGI2: Expect SGI now:\n");
		GIC_SendSGI(SGI2_IRQn, 0b01, 0b00);

		print("Now sending SGI3... (should happen after USARTx exits)\n");
		GIC_SendSGI(SGI3_IRQn, 0b01, 0b00);

		print("< USARTx handler ended: Expect SGI3 now:\n");
	});

	USARTx->CR1 &= ~USART_CR1_UE;
	USARTx->CR1 |= USART_CR1_RE; // enable RX
	USARTx->CR3 |= USART_CR3_RXFTIE;
	USARTx->CR1 |= USART_CR1_UE;
	USARTx->CR1 |= USART_CR1_RXFNEIE;

	print("\nPress a key to trigger USART", UART, " RX IRQ\n");
	print("Or press button USER2\n");

	button_user1_init();
	button_user2_init();

	bool fired = false;

	while (true) {

		// Poll for the button press (TODO: use EXTI to fire an interrupt)
		if (button_user2_pressed()) {
			if (!fired) {
				fired = true;
				GIC_SendSGI(SGI4_IRQn, 0b01, 0b00);
			}
		} else {
			fired = false;
		}

		if (button_user1_pressed()) {
			print("Wrong button\n");
		}
		asm("nop");
	}
}
