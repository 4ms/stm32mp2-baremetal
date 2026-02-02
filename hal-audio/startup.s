.cpu cortex-a35

// Drops to EL1 if started in EL2, sets SP, vectors, clears .bss, calls C.
    .section .text.boot, "ax"
    .global _start
    .global _Reset
_Reset:
    // Mask interrupts
    msr     daifset, #0xf

	// Assume we start in EL1
    bl early_print_el

el1_entry:
    // Set up stack
    ldr     x1, =_stack_start
    mov     sp, x1

    // Install vector table for EL1
    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb

    // Enable FP+SIMD at EL1
	mrs     x1, cpacr_el1
	orr	    x1, x1, #3 << 20        /* FPEN bits: don't trap FPU at EL1 or EL0 */
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

    bl system_init

	mrs x1, sctlr_el1
	orr x1, x1, #0x1000    /* I: bit 12 instruction cache */
	orr x1, x1, #0x0001    /* M: bit 1  MMU enable for EL1 and EL0  */
	orr x1, x1, #0x0004    /* C: bit 2  Cacheability control for data accesses at EL1 and EL0 */
	msr	sctlr_el1, x1

	bl __libc_init_array

    msr     daifclr, #0xf

    // Call main
    bl      main

hang:
    wfe
    b       hang

