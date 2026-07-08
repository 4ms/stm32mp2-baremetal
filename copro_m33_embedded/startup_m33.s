/* Minimal Cortex-M33 startup for the STM32MP257F coprocessor.
 *
 * The A35 loads this image into SRAM2, points CA35SYSCFG->M33_INITSVTOR_CR at
 * the vector table (the M33 runs secure), and releases CPU2 from hold-boot. On
 * reset the M33 loads SP from vector[0] and PC from vector[1] (Reset_Handler).
 *
 * We keep it tiny: enable the FPU, zero .bss, (no-op) copy .data, call main().
 * No interrupts are used by the demo, so only the core exception vectors are
 * populated; every unused slot points at an infinite-loop Default_Handler.
 */

	.syntax unified
	.cpu cortex-m33
	.fpu fpv5-sp-d16
	.thumb

.global g_pfnVectors
.global Default_Handler

.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

	.section .text.Reset_Handler,"ax",%progbits
	.weak Reset_Handler
	.type Reset_Handler, %function
Reset_Handler:
	ldr   sp, =_estack

	/* Boot breadcrumb for the A35: prove the M33 executed its first
	 * instructions by writing a magic word into an unused vector slot
	 * (slot 8, offset 0x20). A second magic goes into slot 9 just before
	 * main(). The A35 polls these after releasing hold-boot (main_a35.cc). */
	ldr   r0, =g_pfnVectors
	ldr   r1, =0xB007C0DE
	str   r1, [r0, #0x20]
	dsb

	/* Enable CP10/CP11 (FPU) full access: SCB->CPACR |= (0xF << 20) */
	ldr   r0, =0xE000ED88
	ldr   r1, [r0]
	orr   r1, r1, #(0xF << 20)
	str   r1, [r0]
	dsb
	isb

	/* Copy .data from LMA to VMA (a no-op here since LMA == VMA). */
	movs  r1, #0
	b     LoopCopyDataInit

CopyDataInit:
	ldr   r3, =_sidata
	ldr   r3, [r3, r1]
	str   r3, [r0, r1]
	adds  r1, r1, #4

LoopCopyDataInit:
	ldr   r0, =_sdata
	ldr   r3, =_edata
	adds  r2, r0, r1
	cmp   r2, r3
	bcc   CopyDataInit

	/* Zero .bss */
	ldr   r2, =_sbss
	b     LoopFillZerobss

FillZerobss:
	movs  r3, #0
	str   r3, [r2], #4

LoopFillZerobss:
	ldr   r3, =_ebss
	cmp   r2, r3
	bcc   FillZerobss

	/* Second breadcrumb: startup completed, entering main() (slot 9). */
	ldr   r0, =g_pfnVectors
	ldr   r1, =0xB007C0DF
	str   r1, [r0, #0x24]
	dsb

	bl    main

LoopForever:
	b     LoopForever

	.size Reset_Handler, .-Reset_Handler

	.section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
	b Infinite_Loop
	.size Default_Handler, .-Default_Handler

	.section .isr_vector,"a",%progbits
	.type g_pfnVectors, %object
g_pfnVectors:
	.word  _estack             /* Top of stack           */
	.word  Reset_Handler       /* Reset                  */
	.word  NMI_Handler         /* NMI                    */
	.word  HardFault_Handler   /* Hard fault             */
	.word  MemManage_Handler   /* MPU fault              */
	.word  BusFault_Handler    /* Bus fault              */
	.word  UsageFault_Handler  /* Usage fault            */
	.word  SecureFault_Handler /* Secure fault           */
	.word  0
	.word  0
	.word  0
	.word  SVC_Handler         /* SVCall                 */
	.word  DebugMon_Handler    /* Debug monitor          */
	.word  0
	.word  PendSV_Handler      /* PendSV                 */
	.word  SysTick_Handler     /* SysTick                */
	.size  g_pfnVectors, .-g_pfnVectors

	.macro weak_handler name
	.weak      \name
	.thumb_set \name,Default_Handler
	.endm

	weak_handler NMI_Handler
	weak_handler HardFault_Handler
	weak_handler MemManage_Handler
	weak_handler BusFault_Handler
	weak_handler UsageFault_Handler
	weak_handler SecureFault_Handler
	weak_handler SVC_Handler
	weak_handler DebugMon_Handler
	weak_handler PendSV_Handler
	weak_handler SysTick_Handler
