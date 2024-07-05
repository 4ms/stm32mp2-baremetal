.cpu cortex-a35

.equ UART_TDR, 0x40010028
.equ STM32_USART2_BASE, 0x400E0000
.equ STM32_USART2_TDR, 0x400E0028

/* serial0: usart2 */
/* pinmux = <STM32_PINMUX('A', 4, AF6)>;  USART2_TX */
/* pinmux = <STM32_PINMUX('A', 8, AF8)>;  USART2_RX */


/* serial1: usart6 */

.section .vector_table, "x"
.global _Reset
.global _start
_Reset:
    b Reset_Handler
    b . /* 0x4  Undefined Instruction */
    b . /* Software Interrupt */
    b .  /* 0xC  Prefetch Abort */
    b . /* 0x10 Data Abort */
    b . /* 0x14 Reserved */
    b . /* 0x18 IRQ */
    b . /* 0x1C FIQ */

.section .text
Reset_Handler:
	/* UART: print 'A' */
	 ldr x4, =STM32_USART2_TDR 
	 mov x0, #65 
	 str x0, [x4] 
	/* b . */
	bl main
