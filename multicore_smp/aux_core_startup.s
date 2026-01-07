.global aux_main
.global aux_core_startup
aux_core_startup:
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
