/* Cortex-M33 firmware for the STM32MP257F, meant to be embedded into the
 * Cortex-A35 firmware and copied into SRAM2 by the A35 (see main_a35.cc).
 *
 * The whole image (vectors + code + rodata + data) is placed contiguously in
 * SRAM2 so the A35 can copy it as a single flat blob. .bss and the stack are
 * NOLOAD, so they don't appear in the binary the A35 embeds.
 *
 * Address note (STM32MP25):
 *   The M33 fetches instructions from the "instruction fetch" alias of the
 *   SRAMs. For SRAM2 that is 0x0A060000 (non-secure) / 0x0E060000 (secure).
 *   We run the M33 *secure* (CA35SYSCFG->M33_TZEN_CR CFG_SECEXT = 1), so the
 *   image is linked at the secure alias 0x0E060000, which is also the value
 *   written into CA35SYSCFG->M33_INITSVTOR_CR.
 *   The A35 sees the same physical SRAM2 at 0x0A060000 (its only alias for
 *   SRAM2), so it copies the blob to 0x0A060000; both addresses reach the
 *   same RAM.
 */

ENTRY(Reset_Handler)

/* SRAM2 is 128 kB. Stack grows down from the top. */
_estack = 0x0E080000;

_Min_Heap_Size  = 0x400;
_Min_Stack_Size = 0x800;

MEMORY
{
	SRAM2 (rwx) : ORIGIN = 0x0E060000, LENGTH = 128K
}

SECTIONS
{
	.isr_vector :
	{
		. = ALIGN(4);
		KEEP(*(.isr_vector))
		. = ALIGN(4);
	} > SRAM2

	.text :
	{
		. = ALIGN(4);
		*(.text)
		*(.text*)
		*(.glue_7)
		*(.glue_7t)
		*(.eh_frame)
		KEEP (*(.init))
		KEEP (*(.fini))
		. = ALIGN(4);
		_etext = .;
	} > SRAM2

	.rodata :
	{
		. = ALIGN(4);
		*(.rodata)
		*(.rodata*)
		. = ALIGN(4);
	} > SRAM2

	.ARM.extab :
	{
		. = ALIGN(4);
		*(.ARM.extab* .gnu.linkonce.armextab.*)
		. = ALIGN(4);
	} > SRAM2

	.ARM :
	{
		. = ALIGN(4);
		__exidx_start = .;
		*(.ARM.exidx*)
		__exidx_end = .;
		. = ALIGN(4);
	} > SRAM2

	.preinit_array :
	{
		. = ALIGN(4);
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array*))
		PROVIDE_HIDDEN (__preinit_array_end = .);
		. = ALIGN(4);
	} > SRAM2

	.init_array :
	{
		. = ALIGN(4);
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array*))
		PROVIDE_HIDDEN (__init_array_end = .);
		. = ALIGN(4);
	} > SRAM2

	.fini_array :
	{
		. = ALIGN(4);
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP (*(SORT(.fini_array.*)))
		KEEP (*(.fini_array*))
		PROVIDE_HIDDEN (__fini_array_end = .);
		. = ALIGN(4);
	} > SRAM2

	/* .data is loaded in place by the A35 (LMA == VMA), so _sidata == _sdata
	 * and the startup copy loop is effectively a no-op. */
	_sidata = LOADADDR(.data);
	.data :
	{
		. = ALIGN(4);
		_sdata = .;
		*(.data)
		*(.data*)
		. = ALIGN(4);
		_edata = .;
	} > SRAM2

	.bss (NOLOAD) :
	{
		. = ALIGN(4);
		_sbss = .;
		__bss_start__ = _sbss;
		*(.bss)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
		__bss_end__ = _ebss;
	} > SRAM2

	._user_heap_stack (NOLOAD) :
	{
		. = ALIGN(8);
		PROVIDE ( end = . );
		PROVIDE ( _end = . );
		. = . + _Min_Heap_Size;
		. = . + _Min_Stack_Size;
		. = ALIGN(8);
	} > SRAM2

	/* Keep libgcc (compiler runtime helpers); drop libc/libm. */
	/DISCARD/ :
	{
		libc.a ( * )
		libm.a ( * )
	}

	.ARM.attributes 0 : { *(.ARM.attributes) }
}
