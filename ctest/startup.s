.cpu cortex-a35
.equ STM32_USART2_TDR, 0x400E0028

// Drops to EL1 if started in EL2, sets SP, vectors, clears .bss, calls C.

    .section .text.boot, "ax"
//    .align  11
    .global _start
    .global _Reset
_Reset:
    // Mask interrupts
    msr     daifset, #0xf

	ldr x4, =STM32_USART2_TDR 
	mov x0, #77
	str x0, [x4] 
	mov x0, #80 
	str x0, [x4] 
	mov x0, #50 
	str x0, [x4] 
	mov x0, #10 
	str x0, [x4] 

    bl early_hello

    // Read CurrentEL (bits[3:2] hold EL)
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    and     x0, x0, #0x3

    cmp     x0, #2
    b.eq    1f
    cmp     x0, #1
    b.eq    2f

    // If EL3 or EL0, just hang (unexpected error)
0:  wfe
    b       0b

// ---- If started in EL2, drop to EL1 ----
1:
	ldr x4, =STM32_USART2_TDR 
	mov x1, '2'
	str x1, [x4] 

    // Set up a SP 
    ldr     x1, =_stack_start
    mov     sp, x1

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

    eret

// ---- EL1 common entry ----
2:
    // If we started in EL1 already, fall through.
el1_entry:
	ldr x4, =STM32_USART2_TDR 
	mov x1, '1'
	str x1, [x4] 

    // Set up stack
    ldr     x1, =_stack_start
    mov     sp, x1

    // Install vector table for EL1
    ldr     x0, =vector_table
    msr     vbar_el1, x0
    isb

    // Enable FP+SIMD at EL1
    mrs     x0, cpacr_el1
    orr     x0, x0, #(3 << 20)      // FPEN=11
    msr     cpacr_el1, x0
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

    // Call C runtime init hook (optional)
    bl      system_init

    // Call main
    bl      main

hang:
    wfe
    b       hang

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

