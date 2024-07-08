OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)

ENTRY(_Reset)

MEMORY
{
    ROM (rx) : ORIGIN = 0x88000000, LENGTH = 1M
}


SECTIONS
{
    .text : {
        *(.text)
        *(.text*)
        . = ALIGN(8);
     } > ROM

}
