OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)

ENTRY(_Reset)

MEMORY
{
    ROM (rx) :   ORIGIN = 0x88000040, LENGTH = 16M
    RAM (rwx):   ORIGIN = 0x90000000, LENGTH = 16M

	/* Non-Secure Boot Aliases */
	B_S_SYSRAM (rw): ORIGIN = 0x0A000000, LENGTH = 256K
	B_S_SRAM1 (rw):  ORIGIN = 0x0A040000, LENGTH = 128K
	B_S_SRAM2 (rw):  ORIGIN = 0x0A060000, LENGTH = 128K
	B_S_RETRAM (rw): ORIGIN = 0x0A080000, LENGTH = 128K
	B_S_VDERAM (rw): ORIGIN = 0x0A0A0000, LENGTH = 128K

	/* Secure Boot Aliases */
	B_NS_SYSRAM (rw): ORIGIN = 0x0E000000, LENGTH = 256K
	B_NS_SRAM1 (rw):  ORIGIN = 0x0E040000, LENGTH = 128K
	B_NS_SRAM2 (rw):  ORIGIN = 0x0E060000, LENGTH = 128K
	B_NS_RETRAM (rw): ORIGIN = 0x0E080000, LENGTH = 128K
	B_NS_VDERAM (rw): ORIGIN = 0x0E0A0000, LENGTH = 128K

	/* Non-Secure Aliases */
	NS_SYSRAM (rw): ORIGIN = 0x20000000, LENGTH = 256K
	NS_SRAM1 (rw):  ORIGIN = 0x20040000, LENGTH = 128K
	NS_SRAM2 (rw):  ORIGIN = 0x20060000, LENGTH = 128K
	NS_RETRAM (rw): ORIGIN = 0x20080000, LENGTH = 128K
	NS_VDERAM (rw): ORIGIN = 0x200A0000, LENGTH = 128K

	/* Secure Aliases */
	S_SYSRAM (rw): ORIGIN = 0x30000000, LENGTH = 256K
	S_SRAM1 (rw):  ORIGIN = 0x30040000, LENGTH = 128K
	S_SRAM2 (rw):  ORIGIN = 0x30060000, LENGTH = 128K
	S_RETRAM (rw): ORIGIN = 0x30080000, LENGTH = 128K
	S_VDERAM (rw): ORIGIN = 0x300A0000, LENGTH = 128K
}

__HEAP_SIZE = 0x40000;

SECTIONS
{
	/* You can test DMA with various SRAM blocks with these: */
	.s_sysram 	 (NOLOAD): { *(.s_sysram) } 	> S_SYSRAM
	.ns_sysram 	 (NOLOAD): { *(.ns_sysram) } 	> NS_SYSRAM
	.b_s_sysram  (NOLOAD): { *(.b_s_sysram) }	> B_S_SYSRAM
	.b_ns_sysram (NOLOAD): { *(.b_ns_sysram) } 	> B_NS_SYSRAM

	.s_sram1 	 (NOLOAD): { *(.s_sram1) } 		> S_SRAM1
	.ns_sram1 	 (NOLOAD): { *(.ns_sram1) } 	> NS_SRAM1
	.b_s_sram1 	 (NOLOAD): { *(.b_s_sram1) }	> B_S_SRAM1
	.b_ns_sram1  (NOLOAD): { *(.b_ns_sram1) } 	> B_NS_SRAM1

	.s_sram2 	 (NOLOAD): { *(.s_sram2) } 		> S_SRAM2
	.ns_sram2 	 (NOLOAD): { *(.ns_sram2) } 	> NS_SRAM2
	.b_s_sram2 	 (NOLOAD): { *(.b_s_sram2) }	> B_S_SRAM2
	.b_ns_sram2  (NOLOAD): { *(.b_ns_sram2) } 	> B_NS_SRAM2

	.s_retram 	 (NOLOAD): { *(.s_retram) } 	> S_RETRAM
	.ns_retram 	 (NOLOAD): { *(.ns_retram) } 	> NS_RETRAM
	.b_s_retram	 (NOLOAD): { *(.b_s_retram) }	> B_S_RETRAM
	.b_ns_retram (NOLOAD): { *(.b_ns_retram) } 	> B_NS_RETRAM

	/* Not working? */
	.s_vderam 	 (NOLOAD): { *(.s_vderam) } 	> S_VDERAM
	.ns_vderam 	 (NOLOAD): { *(.ns_vderam) } 	> NS_VDERAM
	.b_s_vderam	 (NOLOAD): { *(.b_s_vderam) }	> B_S_VDERAM
	.b_ns_vderam (NOLOAD): { *(.b_ns_vderam) } 	> B_NS_VDERAM

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
