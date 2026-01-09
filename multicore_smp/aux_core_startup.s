.global aux_main
.global aux_core_startup
aux_core_startup:
    // Mask interrupts
    msr daifset, #0xf

    // Set up stack
    ldr  x1, =_cpu1_stack_start
    mov  sp, x1

    // Install vector table for EL1
    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb

    // Enable FP+SIMD at EL1
	mrs     x1, cpacr_el1
	orr	    x1, x1, #3 << 20        /* FPEN bits: don't trap FPU at EL1 or EL0 */
	msr	    cpacr_el1, x1
	isb

	bl mmu_enable
	bl IRQ_Initialize

	// Enable caches
	mrs x1, sctlr_el1
	orr x1, x1, #0x1000    /* I: bit 12 instruction cache */
	orr x1, x1, #0x0001    /* M: bit 1  MMU enable for EL1 and EL0  */
	orr x1, x1, #0x0004    /* C: bit 2  Cacheability control for data accesses at EL1 and EL0 */
	msr	sctlr_el1, x1


    // Unmask interrupts
    msr daifclr, #0xf

    bl   aux_main

aux_hang:
    wfe
    b       aux_hang
