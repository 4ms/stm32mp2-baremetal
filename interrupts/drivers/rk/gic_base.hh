#error This file is for referenced and should not be used

#define GIC_BASE 0x4AC00000UL
#define GIC_DISTRIBUTOR_BASE (GIC_BASE + 0x10000UL)

// What is this on the mp2?
// #define GIC_REDISTRIBUTOR_BASE (GIC_BASE + 0x60000)

#define GIC_INTERFACE_BASE (GIC_BASE + 0x20000UL)

/* Number of implemented interrupt lines */
/* to be computed from GICD_TYPER.ITLinesNumber, bits [4:0] = NUM_SPIS/32 */
/* 0xC = 12 --> (12+1)*32 = 416 */
#define IRQ_GIC_LINE_COUNT (416U)

/* Number of priority bits according to A35 security state */
#if defined(A35_NON_SECURE)
#define GIC_PRIO_BITS 4U
#else
#define GIC_PRIO_BITS 5U
#endif /* defined(A35_NON_SECURE) */
