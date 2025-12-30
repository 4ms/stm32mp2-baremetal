.cpu cortex-a35

.equ STM32_USART2_TDR, 0x400E0028

/* serial1: usart6 */

.global _Reset
.global _start
_Reset:
	/* UART: print 'MP2\n' */
	ldr x4, =STM32_USART2_TDR 
	mov x0, #77
	str x0, [x4] 
	mov x0, #80 
	str x0, [x4] 
	mov x0, #50 
	str x0, [x4] 
	mov x0, #10 
	str x0, [x4] 

	ldr	x0, =(_stack_start)
	bic	sp, x0, #0xf	/* 16-byte alignment for ABI compliance */

	b main

.section .vector_table, "x"
	b .
	b .
	b .
	b .
	b .
	b .
	b .
	b .
	b .
	b .
	b .
	b .
	b .
	b .
	b .
	b .


