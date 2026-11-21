    .syntax unified
    .arm
    .section .crt0, "ax"
    .global _start

_start:
    b _start_code

    /* 0x08000004: 156 bytes of Nintendo logo space (patched by build tool) */
    .space 156, 0

    /* 0x080000A0: Game Title (12 bytes) */
    .ascii "GBA HW CHECK"

    /* 0x080000AC: Game Code (4 bytes) - V49E (Drill Dozer Hardware Rumble) */
    .ascii "V49E"

    /* 0x080000B0: Maker Code (2 bytes) */
    .ascii "01"

    /* 0x080000B2: Fixed 0x96 */
    .byte 0x96

    /* 0x080000B3: Unit code */
    .byte 0x00

    /* 0x080000B4: Device type */
    .byte 0x00

    /* 0x080000B5: Reserved 7 bytes */
    .space 7, 0

    /* 0x080000BC: Version */
    .byte 0x00

    /* 0x080000BD: Complement check */
    .byte 0x00

    /* 0x080000BE: Reserved 2 bytes */
    .byte 0x00, 0x00

    /* 0x080000C0: Reserved Cartridge I/O space (0x080000C4..0x080000C9 must be zero-filled for GPIO/RTC/Rumble) */
    .space 32, 0

    /* 0x080000E0: Execution begins here */
    .align 4
    .global _start_code
_start_code:
    /* Disable interrupts and switch to System Mode */
    msr cpsr_c, #0xDF
    ldr sp, =__sp_usr

    /* Switch to IRQ Mode and set IRQ stack */
    msr cpsr_c, #0xD2
    ldr sp, =__sp_irq

    /* Return to System Mode with IRQs enabled or ready */
    msr cpsr_c, #0x1F
    ldr sp, =__sp_usr

    /* Copy .data from ROM (LMA) to EWRAM (VMA) */
    ldr r0, =__data_lma
    ldr r1, =__data_start
    ldr r2, =__data_end
1:
    cmp r1, r2
    ldrhlt r3, [r0], #2
    strhlt r3, [r1], #2
    blt 1b

    /* Copy .iwram from ROM (LMA) to IWRAM (VMA) */
    ldr r0, =__iwram_lma
    ldr r1, =__iwram_start
    ldr r2, =__iwram_end
2:
    cmp r1, r2
    ldrhlt r3, [r0], #2
    strhlt r3, [r1], #2
    blt 2b

    /* Clear .bss section */
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
3:
    cmp r0, r1
    strhlt r2, [r0], #2
    blt 3b

    /* Set up WAITCNT: default 3,1 cycles for ROM WS0, enable prefetch buffer */
    ldr r0, =0x04000204
    ldr r1, =0x4317          /* WS0: 3,1 waitstates, Prefetch enabled */
    strh r1, [r0]

    /* Call C main function */
    ldr r0, =main
    bx r0

    /* Halt loop if main ever returns */
_hang:
    b _hang

    .pool
