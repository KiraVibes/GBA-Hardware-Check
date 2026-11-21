    .syntax unified
    .arm
    .section .crt0, "ax"
    .global _start

_start:
    b _start_code

    /* 0x02000004: 156 bytes of Nintendo logo space (patched by build tool) */
    .space 156, 0

    /* 0x020000A0: Game Title (12 bytes) */
    .ascii "GBA HW CHECK"

    /* 0x020000AC: Game Code (4 bytes) */
    .ascii "MBDC"

    /* 0x020000B0: Maker Code (2 bytes) */
    .ascii "01"

    /* 0x020000B2: Fixed 0x96 */
    .byte 0x96

    /* 0x020000B3: Unit code */
    .byte 0x00

    /* 0x020000B4: Device type */
    .byte 0x00

    /* 0x020000B5: Reserved 7 bytes */
    .space 7, 0

    /* 0x020000BC: Version */
    .byte 0x00

    /* 0x020000BD: Complement check */
    .byte 0x00

    /* 0x020000BE: Reserved 2 bytes */
    .byte 0x00, 0x00

    /* 0x020000C0: MultiBoot Execution Entry Point */
    .align 4
    .global _start_code
_start_code:
    /* Switch to System Mode, disable IRQ */
    msr cpsr_c, #0xDF
    ldr sp, =__sp_usr

    /* Clear BSS section */
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
1:
    cmp r0, r1
    strlo r2, [r0], #4
    blo 1b

    /* Call main client code */
    bl mb_main

_hang:
    b _hang
