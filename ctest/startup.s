.cpu cortex-a35
.equ STM32_USART2_TDR, 0x400E0028
.equ STM32_USART6_TDR, 0x40220028

// Drops to EL1 if started in EL2, sets SP, vectors, clears .bss, calls C.
    .section .text.boot, "ax"
    .global _start
    .global _Reset
_Reset:
    // Mask interrupts
    msr     daifset, #0xf

    // Set up a SP 
    ldr     x0, =_stack_start
	bic		sp, x0, #0xf	// 16-byte alignment 

	// Print "MP2"
	ldr x4, =STM32_USART6_TDR 
	mov x0, #77
	str x0, [x4] 
	mov x0, #80 
	str x0, [x4] 
	mov x0, #50 
	str x0, [x4] 
	mov x0, #10 
	str x0, [x4] 

    bl early_print_el

    // Read CurrentEL
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    and     x0, x0, #0x3

    cmp     x0, #2
    b.eq    el2_entry
    cmp     x0, #1
    b.eq    el1_entry

// Hang if EL0
	cmp 	x0, #0
	b.eq    .

// EL3 Entry:
	mov x0, 'E'
	bl putchar_s
	mov x0, 'L'
	bl putchar_s
	mov x0, '3'
	bl putchar_s
	mov x0, '\n'
	bl putchar_s

    // Install vector table for EL3
    ldr     x0, =vector_table
    msr     vbar_el3, x0
    isb
	
	// Enable FP/SIMD at EL3
	mrs		x0, cptr_el3
	bic 	x0, x0, #(1<<10)   // Do not trap FPU/SIMD
	bic 	x0, x0, #(1<<8)    // Do not trap SVE
	msr 	cptr_el3, x0

    b       doneinit

// ---- EL2 entry: drop to EL1 ----
el2_entry:
	mov x0, 'E'
	bl putchar_s
	mov x0, 'L'
	bl putchar_s
	mov x0, '2'
	bl putchar_s
	mov x0, '\n'
	bl putchar_s


    // Configure EL1 to run AArch64, with interrupts masked on entry
    // SPSR_EL2: M[3:0]=0101 (EL1h), DAIF=1111
    mov     x2, #(0x5)              // EL1h
    orr     x2, x2, #(0xF << 6)     // mask D,A,I,F
    msr     spsr_el2, x2

    // Set ELR_EL2 to EL1 entry point and provide SP_EL1
    adr     x3, el1_entry
    msr     elr_el2, x3
    msr     sp_el1, x1

    // Ensure EL1 is AArch64 (RW=1 in HCR_EL2). Most U-Boot setups already do this,
    // but it doesn't hurt to force it.
    mrs     x4, hcr_el2
    orr     x4, x4, #(1 << 31)      // HCR_EL2.RW = 1 => EL1 is AArch64
    msr     hcr_el2, x4
    isb

	mov x0, '>'
	bl putchar_s

    eret

// ---- EL1 entry ----
el1_entry:
	mov x0, 'E'
	bl putchar_s
	mov x0, 'L'
	bl putchar_s
	mov x0, '2'
	bl putchar_s
	mov x0, '\n'
	bl putchar_s

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

// Init finished:
doneinit:

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

	bl led1_init
	bl led3_init


	bl led1_on
	bl delay_100
	bl led1_off

	// TODO: el3 mmu
    // bl mmu_enable

	// mrs x1, sctlr_el1
	// orr x1, x1, #0x1000    /* I: bit 12 instruction cache */
	// orr x1, x1, #0x0001    /* M: bit 1  MMU enable for EL1 and EL0  */
	// orr x1, x1, #0x0004    /* C: bit 2  Cacheability control for data accesses at EL1 and EL0 */
	// msr	sctlr_el1, x1

	bl led3_on
	bl delay_100
	bl led3_off

	bl __libc_init_array

    // Call main
    bl      main

hang:
    wfe
    b       hang

delay_100:
	mov x8, #0x8000000 //about 200ms
3:  subs x8, x8, #1
	b.ne 3b
	ret


// ---- Vector table ----
//   .align  11
vector_table:
    // 16 entries
    // Current EL with SP0
    b sync_sp0
    b irq_sp0
    b fiq_sp0
    b serr_sp0
    // Current EL with SPx
    b sync_spx
    b irq_spx
    b fiq_spx
    b serr_spx
    // Lower EL using AArch64
    b sync_lower_a64
    b irq_lower_a64
    b fiq_lower_a64
    b serr_lower_a64
    // Lower EL using AArch32
    b sync_lower_a32
    b irq_lower_a32
    b fiq_lower_a32
    b serr_lower_a32

// Default handlers
sync_sp0:          b .
irq_sp0:           b .
fiq_sp0:           b .
serr_sp0:          b .
sync_spx:          b .
irq_spx:           b .
fiq_spx:           b .
serr_spx:          b .
sync_lower_a64:    b .
irq_lower_a64:     b .
fiq_lower_a64:     b .
serr_lower_a64:    b .
sync_lower_a32:    b .
irq_lower_a32:     b .
fiq_lower_a32:     b .
serr_lower_a32:    b .

    .size vector_table, . - vector_table

