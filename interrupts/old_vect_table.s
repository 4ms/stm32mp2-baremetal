vector_table:
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
sync_sp0:         
	mov x0, 'S'
	bl early_putc
	b .
irq_sp0: 
	mov x0, 'I'
	bl early_putc
	b .
fiq_sp0:
	mov x0, 'F'
	bl early_putc
	b .
serr_sp0:
	mov x0, 'E'
	bl early_putc
	b .

sync_spx:         
	mov x0, 's'
	bl early_putc
	b .
irq_spx: 
	mov x0, 'i'
	bl early_putc
	b .
fiq_spx:
	mov x0, 'f'
	bl early_putc
	b .
serr_spx:
	mov x0, 'e'
	bl early_putc
	b .

sync_lower_a64:         
	mov x0, 'Y'
	bl early_putc
	b .
irq_lower_a64: 
	mov x0, 'R'
	bl early_putc
	b .
fiq_lower_a64:
	mov x0, 'Q'
	bl early_putc
	b .
serr_lower_a64:
	mov x0, 'O'
	bl early_putc
	b .

sync_lower_a32:         
	mov x0, 'y'
	bl early_putc
	b .
irq_lower_a32: 
	mov x0, 'r'
	bl early_putc
	b .
fiq_lower_a32:
	mov x0, 'q'
	bl early_putc
	b .
serr_lower_a32:
	mov x0, 'o'
	bl early_putc
	b .

    .size vector_table, . - vector_table

