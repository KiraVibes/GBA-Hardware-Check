/* Linker script for GBA MultiBoot Client (runs entirely from EWRAM 0x02000000) */

OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    ewram (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
    iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 32K
}

SECTIONS
{
    .text :
    {
        *(.crt0)
        *(.text .text.*)
        *(.glue_7)
        *(.glue_7t)
        *(.rodata .rodata.*)
        *(.data .data.*)
        . = ALIGN(4);
    } > ewram

    .bss (NOLOAD) :
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > ewram

    __sp_usr = 0x03007F00;
}
