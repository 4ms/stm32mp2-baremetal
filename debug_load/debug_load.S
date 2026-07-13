.cpu cortex-a35

#if UART==6
.equ STM32_USART_TDR, 0x40220028
#elif UART==1
.equ STM32_USART_TDR, 0x40330028
#else
.equ STM32_USART_TDR, 0x400E0028
#endif

.global _Reset
.global _start
_Reset:
// Leave a clean, coherent cache state before handing off.
// Disable the D-cache and MMU, clean+invalidate the whole D-cache to memory, then drop the I-cache/TLBs.
    mrs     x0, sctlr_el3
    bic     x0, x0, #(1 << 0)       // M = 0: MMU off
    bic     x0, x0, #(1 << 2)       // C = 0: D-cache off (no new allocations)
    bic     x0, x0, #(1 << 12)      // I = 0: I-cache off
    msr     sctlr_el3, x0
    isb

    // Clean+invalidate all data/unified cache levels by set/way (to PoC).
    mrs     x0, clidr_el1
    and     w3, w0, #0x07000000     // w3 = Level of Coherence << 1
    lsr     w3, w3, #23
    cbz     w3, flush_done
    mov     w10, #0                 // w10 = 2 * cache level
    mov     w8, #1
flush_level:
    add     w2, w10, w10, lsr #1    // w2 = 3 * cache level
    lsr     w1, w0, w2              // w1 = cache type field for this level
    and     w1, w1, #0x7
    cmp     w1, #2
    b.lt    flush_skip              // <2 => no data/unified cache here
    msr     csselr_el1, x10         // select this cache level
    isb
    mrs     x1, ccsidr_el1
    and     w2, w1, #7
    add     w2, w2, #4              // w2 = log2(line bytes)
    ubfx    w4, w1, #3, #10         // w4 = ways - 1
    clz     w5, w4                  // w5 = way shift (32 - log2(ways))
    lsl     w9, w4, w5              // w9 = ways-1, positioned in way field
    lsl     w16, w8, w5             // w16 = 1 in way field (way decrement)
flush_way:
    ubfx    w7, w1, #13, #15        // w7 = sets - 1
    lsl     w7, w7, w2              // position in set field
    lsl     w17, w8, w2             // w17 = 1 in set field (set decrement)
flush_set:
    orr     w11, w10, w9            // level | way
    orr     w11, w11, w7            // level | way | set
    dc      cisw, x11               // clean & invalidate this line
    subs    w7, w7, w17
    b.ge    flush_set
    subs    w9, w9, w16
    b.ge    flush_way
flush_skip:
    add     w10, w10, #2            // next cache level
    cmp     w3, w10
    b.gt    flush_level
flush_done:
    mov     x10, #0
    msr     csselr_el1, x10
    dsb     sy
    isb

    ic      iallu                   // invalidate all I-cache
    tlbi    alle3                   // invalidate EL3 TLBs
    dsb     sy
    isb


printMP2:
    ldr     x4, =STM32_USART_TDR
    mov     x1, 'R'
    str     x1, [x4]
	bl delay
    mov     x1, 'e'
    str     x1, [x4]
	bl delay
    mov     x1, 'a'
    str     x1, [x4]
	bl delay
    mov     x1, 'd'
    str     x1, [x4]
	bl delay
    mov     x1, 'y'
    str     x1, [x4]
	bl delay
    mov     x1, '\n'
    str     x1, [x4]
	bl delay

delay:
    mov     w0, #0x100000
delay_loop:
    subs    w0, w0, #0x1
    b.ne    delay_loop
	ret
	

loopforever:
    b       .
