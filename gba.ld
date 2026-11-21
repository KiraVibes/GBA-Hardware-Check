/* Linker script for Game Boy Advance ROM */

OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    ewram (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
    iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 32K
}

SECTIONS
{
    /* ROM Header & code starts at 0x08000000 */
    .text :
    {
        *(.crt0)
        *(.text .text.*)
        *(.glue_7)
        *(.glue_7t)
        *(.gnu.linkonce.t.*)
        . = ALIGN(4);
    } > rom

    .rodata :
    {
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r.*)
        . = ALIGN(4);
    } > rom

    /* IWRAM sections: code or data that must run at maximum speed in internal 32-bit RAM */
    __iwram_lma = LOADADDR(.iwram);
    .iwram :
    {
        __iwram_start = .;
        *(.iwram .iwram.*)
        . = ALIGN(4);
        __iwram_end = .;
    } > iwram AT > rom

    /* Initialized data placed in EWRAM (copied from ROM at boot) */
    __data_lma = LOADADDR(.data);
    .data :
    {
        __data_start = .;
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(4);
        __data_end = .;
    } > ewram AT > rom

    /* Uninitialized data (zeroed at boot) */
    .bss (NOLOAD) :
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > ewram

    /* Stack pointer marks top of IWRAM (leaving upper 0x100 bytes for BIOS) */
    __sp_usr = 0x03007F00;
    __sp_irq = 0x03007FA0;
    __sp_svc = 0x03007FE0;

    /* Discard unnecessary debugging or unwind sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.ARM.extab*)
        *(.ARM.exidx*)
    }
}
