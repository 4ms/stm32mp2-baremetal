// secondary_entry.S
.global aux_main
secondary_entry:
    ldr  x0, =_cpu1_stack_start
    mov  sp, x0

    dsb  sy
    isb
    ic   iallu
    dsb  sy
    isb

    bl   aux_main
1:  wfe
    b    1b
