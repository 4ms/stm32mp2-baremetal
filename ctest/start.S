.cpu cortex-a35

#if UART==6
.equ STM32_USART6_TDR, 0x40220028
.equ STM32_USART_TDR, STM32_USART6_TDR
#else
.equ STM32_USART2_TDR, 0x400E0028
.equ STM32_USART_TDR, STM32_USART2_TDR
#endif

// Drops to EL1 if started in EL2, sets SP, vectors, clears .bss
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
	ldr x4, =STM32_USART_TDR 
	mov x0, 'M'
	str x0, [x4] 
	mov x0, 'P'
	str x0, [x4] 
	mov x0, '2'
	str x0, [x4] 
	mov x0, '\n'
	str x0, [x4] 

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

// Jump to correct entry for our exception level
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    and     x0, x0, #0x3

    cmp     x0, #3
    b.eq    el3_entry
    cmp     x0, #2
    b.eq    el2_entry
    cmp     x0, #1
    b.eq    el1_entry

// Hang if EL0
	cmp 	x0, #0
	b.eq    .

// ---- EL3 Entry ----
el3_entry:
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
	
    // Configure EL1 to run AArch64, with interrupts masked on entry
    // SPSR_EL3: M[3:0]=0101 (EL1h), DAIF=1111
    mov     x2, #(0b0101)              // EL1h
    orr     x2, x2, #(0xF << 6)     // mask D,A,I,F
    msr     spsr_el3, x2

    // Set ELR_EL3 to EL1 entry point and provide SP_EL1
    adr     x3, el1_entry
    msr     elr_el3, x3
    ldr     x1, =_stack_start
    msr     sp_el1, x1
	isb

    // Ensure EL1 is AArch64 (RW=1 in SCR_EL3)
    mrs     x4, scr_el3
    orr     x4, x4, #(1 << 10)
    msr     scr_el3, x4
    isb
    mrs     x4, hcr_el2
    orr     x4, x4, #(1 << 31)
    msr     hcr_el2, x4
	isb

	mov x0, '>'
	bl putchar_s
	dsb nsh
	isb

    eret


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
    ldr     x1, =_stack_start
    msr     sp_el1, x1

    // Ensure EL1 is AArch64 (RW=1 in HCR_EL2)
    mrs     x4, hcr_el2
    orr     x4, x4, #(1 << 31)
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
	mov x0, '1'
	bl putchar_s
	mov x0, '\n'
	bl putchar_s

    // Install vector table for EL1
    ldr     x0, =vector_table
    msr     vbar_el1, x0
    isb

// Init finished:
doneinit:

	bl led1_init
	bl led1_on
	bl delay_100
	bl led1_off

	bl __libc_init_array

	bl led3_init
	bl led3_on
	bl delay_100
	bl led3_off

// Call main
    bl      main

hang:
    wfe
    b       hang

delay_100:
	mov x8, #0x1000000 //about 100-200ms with caches disabled
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

