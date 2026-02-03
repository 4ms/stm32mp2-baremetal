OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)

ENTRY(_Reset)

MEMORY
{
    ROM (rx) :   ORIGIN = 0x88000040, LENGTH = 16M
    RAM (rwx):   ORIGIN = 0x90000000, LENGTH = 16M
    NCRAM (rwx):   ORIGIN = 0xC0000000, LENGTH = 16M

	/* Secure Boot Aliases */
	B_S_VDERAM (rw): ORIGIN = 0x0A0A0000, LENGTH = 128K
	B_S_RETRAM (rw): ORIGIN = 0x0A080000, LENGTH = 128K
	B_S_SRAM2 (rw):  ORIGIN = 0x0A060000, LENGTH = 128K
	B_S_SRAM1 (rw):  ORIGIN = 0x0A040000, LENGTH = 128K
	B_S_SYSRAM (rw): ORIGIN = 0x0A000000, LENGTH = 256K

	/* Non-Secure Boot Aliases */
	B_NS_VDERAM (rw): ORIGIN = 0x0E0A0000, LENGTH = 128K
	B_NS_RETRAM (rw): ORIGIN = 0x0E080000, LENGTH = 128K
	B_NS_SRAM2 (rw):  ORIGIN = 0x0E060000, LENGTH = 128K
	B_NS_SRAM1 (rw):  ORIGIN = 0x0E040000, LENGTH = 128K
	B_NS_SYSRAM (rw): ORIGIN = 0x0E000000, LENGTH = 256K

	/* Non-Secure Aliases */
	NS_VDERAM (rw): ORIGIN = 0x200A0000, LENGTH = 128K
	NS_RETRAM (rw): ORIGIN = 0x20080000, LENGTH = 128K
	NS_SRAM2 (rw):  ORIGIN = 0x20060000, LENGTH = 128K
	NS_SRAM1 (rw):  ORIGIN = 0x20040000, LENGTH = 128K
	NS_SYSRAM (rw): ORIGIN = 0x20000000, LENGTH = 256K

	/* Secure Aliases */
	S_VDERAM (rw): ORIGIN = 0x300A0000, LENGTH = 128K
	S_RETRAM (rw): ORIGIN = 0x30080000, LENGTH = 128K
	S_SRAM2 (rw):  ORIGIN = 0x30060000, LENGTH = 128K
	S_SRAM1 (rw):  ORIGIN = 0x30040000, LENGTH = 128K
	S_SYSRAM (rw): ORIGIN = 0x30000000, LENGTH = 256K


}

__HEAP_SIZE = 0x40000;

SECTIONS
{
	/* Not being used, but you can test DMA with SRAM with this:*/
	.ddma (NOLOAD): {
		*(.ddma)
		*(.ddma*)
	} > B_NS_SRAM2

	.noncache (NOLOAD): {
		*(.noncache)
		*(.noncache*)
	} > NCRAM

    .text : {
        *(.vector_table)
        *(.text)
        *(.text*)
					/* Todo: check if we need the next 3 lines */
		*(.glue_7)         /* glue arm to thumb code */
		*(.glue_7t)        /* glue thumb to arm code */
		*(.eh_frame)

		KEEP (*(.init)) /* libc ctors */
		KEEP (*(.fini)) /* libc dtors */

        . = ALIGN(8);
     } > ROM

	/* .rodata sections (constants, strings, etc.) */
	.rodata :
	{
		. = ALIGN(8);
		*(.rodata)         
		*(.rodata*) 
		. = ALIGN(8);
	} > ROM

	/* used for unwinding (probably not used, but is ignored if your app doens't use exceptions */
	.ARM.extab	 : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >ROM
	.ARM : {
		__exidx_start = .;
		*(.ARM.exidx*)
		__exidx_end = .;
	} >ROM

	.preinit_array :
	{
		. = ALIGN(8);
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array*))
		PROVIDE_HIDDEN (__preinit_array_end = .);
		. = ALIGN(8);
	} > ROM

	.init_array :
	{
		. = ALIGN(8);
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array*))
		PROVIDE_HIDDEN (__init_array_end = .);
		. = ALIGN(8);
	} > ROM

	.fini_array :
	{
		. = ALIGN(8);
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP (*(SORT(.fini_array.*)))
		KEEP (*(.fini_array*))
		PROVIDE_HIDDEN (__fini_array_end = .);
		. = ALIGN(8);
	} > ROM


    _text_end = .;

    /* .data : AT(ADDR(.text) + SIZEOF(.text)) */
    .data : /*AT(_text_end)*/
    {
        . = ALIGN(8);
        _data_start = .;
        *(.data)
        *(.data*)
        . = ALIGN(8);
        _data_end = .;
    } > RAM

    .bss : {
        _bss_start = .;
        *(.bss)
        *(.bss*)   /* required for some malloc calls */
		*(COMMON)  /* required for libc, such as __lock___atexit_recursive_mutex */
        . = ALIGN(8);
        _bss_end = .;
    } > RAM

	.heap (NOLOAD):
	{
		. = ALIGN(8);
		_sheap = .;
		. += __HEAP_SIZE;
		_eheap = .;
	} > RAM

    _fiq_stack_start = ADDR(.heap) + SIZEOF(.heap);
    _fiq_stack_end = _fiq_stack_start + 0x1000; /* 64 KB */

    _irq_stack_start = _fiq_stack_end;
    _irq_stack_end = _irq_stack_start + 0x1000; /* 64 KB */

    _stack_start = _irq_stack_end;
    _stack_end = _stack_start + 0x1000; /* 64 KB */

    _irq_stack_size = _irq_stack_end - _irq_stack_start;
    _fiq_stack_size = _fiq_stack_end - _fiq_stack_start;

	/*
	.dma_buffer (NOLOAD) :
	{
		. = ALIGN(8);
		*(.dma_buffer)
		*(.dma_buffer*)
		. = ALIGN(8);
	} > SRAM3
	*/
	

  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

	.ARM.attributes 0 : { *(.ARM.attributes) }
}
