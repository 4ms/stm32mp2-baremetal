.global aux_main
.global aux_core_startup
aux_core_startup:
    // Mask interrupts
    msr daifset, #0xf

    // Set up stack
    ldr  x0, =_cpu1_stack_start
    mov  sp, x0

    // Install vector table for EL1
    ldr     x0, =vector_table
    msr     vbar_el1, x0
    isb

    // Enable FP+SIMD at EL1
	mrs     x1, cpacr_el1
	orr	    x1, x1, #3 << 20        /* FPEN bits: don't trap FPU at EL1 or EL0 */
	orr	    x1, x1, #3 << 22        /* Don't trap SIMD at EL1 or EL0*/
	msr	    cpacr_el1, x1
	isb

	// mmu enable?

	// Enable caches
	mrs x1, sctlr_el1
	orr x1, x1, #0x1000    /* I: bit 12 instruction cache */
	orr x1, x1, #0x0001    /* M: bit 1  MMU enable for EL1 and EL0  */
	orr x1, x1, #0x0004    /* C: bit 2  Cacheability control for data accesses at EL1 and EL0 */
	msr	sctlr_el1, x1

	// Is this needed?
    dsb  sy
    isb
    ic   iallu
    dsb  sy
    isb

    // Unmask interrupts
    msr daifclr, #0xf

    bl   aux_main
1:  wfe
    b    1b
