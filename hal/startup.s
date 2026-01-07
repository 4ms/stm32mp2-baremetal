.cpu cortex-a35
.equ STM32_USART2_TDR, 0x400E0028

// Drops to EL1 if started in EL2, sets SP, vectors, clears .bss, calls C.
    .section .text.boot, "ax"
    .global _start
    .global _Reset
_Reset:
    // Mask interrupts
    msr     daifset, #0xf

	bl led1_init
	bl led3_init

	// Assume we start in EL1
    bl early_print_el

el1_entry:
    // Set up stack
    ldr     x1, =_stack_start
    mov     sp, x1

// Route IRQ, FIQ, and SError to EL2
//	mrs x0, hcr_el2
//	orr x0, x0, #(1<<5)
//	orr x0, x0, #(1<<4)
//	orr x0, x0, #(1<<3)
//	msr hcr_el2, x0

    // Install vector table for EL1
    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb

// Enable System Register access (ICC_*_EL1 system registers)
	// mov x1, xzr
	//orr x1, x1, #(1<<3)  // Enable EL0 to use ICC_SER_EL1
	// orr x1, x1, #(1<<2)  // Disable IRQ bypass
	// orr x1, x1, #(1<<1)  // Disable FIQ bypass
	// orr x1, x1, #(1<<0)  // System Register Enable
	// msr icc_sre_el1, x1

    // Enable FP+SIMD at EL1
	mrs     x1, cpacr_el1
	orr	    x1, x1, #3 << 20        /* FPEN bits: don't trap FPU at EL1 or EL0 */
	// orr	    x1, x1, #3 << 22        /* Don't trap SIMD at EL1 or EL0*/
	msr	    cpacr_el1, x1
	isb

    // Clear .bss
    ldr     x0, =_bss_start
    ldr     x1, =_bss_end
    mov     x2, #0
bss_loop:
    cmp     x0, x1
    b.hs    bss_done
    str     x2, [x0], #8
    b       bss_loop
bss_done:

	bl led1_on
	bl delay_100
	bl led1_off

    bl system_init

	mrs x1, sctlr_el1
	orr x1, x1, #0x1000    /* I: bit 12 instruction cache */
	orr x1, x1, #0x0001    /* M: bit 1  MMU enable for EL1 and EL0  */
	orr x1, x1, #0x0004    /* C: bit 2  Cacheability control for data accesses at EL1 and EL0 */
	msr	sctlr_el1, x1

	bl led3_on
	bl delay_100
	bl led3_off

	bl __libc_init_array

    msr     daifclr, #0xf

    // Call main
    bl      main

hang:
    wfe
    b       hang

delay_100:
	mov x8, #0x2000000 //about 50ms
3:  subs x8, x8, #1
	b.ne 3b
	ret

