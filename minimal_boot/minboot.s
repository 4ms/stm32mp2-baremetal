.cpu cortex-a35

.equ STM32_USART2_TDR, 0x400E0028

.global _Reset
.global _start
_Reset:
	nop

delay:
	mov     w0, #0x30000000
    subs    w0, w0, #0x1
    b.ne    delay

printM:
	/* UART: print 'M' */
	ldr x4, =STM32_USART2_TDR 
	mov x0, #77
	str x0, [x4] 

loopforever:
	b .

