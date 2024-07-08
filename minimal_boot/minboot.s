.cpu cortex-a35

.equ STM32_USART2_TDR, 0x400E0028

.global _Reset
.global _start
_Reset:
	ldr     x4, =STM32_USART2_TDR 

printMP2:
	/* UART: print 'M' */
	mov     x1, #77
	str     x1, [x4] 

	/* UART: print 'P' */
	mov     x1, #80
	str     x1, [x4] 

	/* UART: print '2' */
	mov     x1, #50
	str     x1, [x4] 

	/* UART: print '\n' */
	mov     x1, #10
	str     x1, [x4] 

	mov     w0, #0x4000000
delay:
    subs    w0, w0, #0x1
    b.ne    delay

loopforever:
	b       printMP2
	

