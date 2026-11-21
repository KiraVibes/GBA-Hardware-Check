#include "../include/multiboot.h"
#include <string.h>

#define PALETTE_DATA 0x93  /* Default palette for MultiBoot intro logo */

static inline void mb_delay_us(int us) {
    /* At 16.78 MHz, 1 us is ~17 cycles */
    volatile int cycles = us * 4;
    while (cycles-- > 0);
}

static u16 mb_transfer(u16 data, u16* out_slave1) {
    if (REG_SIOCNT & 0x0040) {
        REG_SIOCNT = 0x2003;
    }
    REG_SIOMLT_SEND = data;
    REG_SIOCNT = 0x2083; /* 115200 baud, Start Multi-Player transfer */

    int timeout = 12000;
    while ((REG_SIOCNT & 0x0080) && --timeout > 0);

    mb_delay_us(40); /* GBATEK recommends ~36 us delay between transfers */

    if (timeout > 0 && !(REG_SIOCNT & 0x0040)) {
        u16 rx1 = REG_SIOMULTI1;
        if (out_slave1) *out_slave1 = rx1;
        return rx1;
    }
    return 0xFFFF;
}

static int swi_multiboot_call(MultiBootParam* param) {
    register MultiBootParam* r0 asm("r0") = param;
    register int r1 asm("r1") = 1; /* 1 = MultiPlay mode */
    asm volatile(
        "swi 0x250000"
        : "+r"(r0), "+r"(r1)
        :
        : "r2", "r3", "r12", "lr", "cc", "memory"
    );
    return (int)r0;
}

MultiBootDetectResult multiboot_detect_slave(u16* out_rx_word) {
    /* Send burst of probe commands (0x6200) to sync with BIOS or detect active client */
    for (int t = 0; t < 16; t++) {
        u16 rx1 = 0;
        u16 res = mb_transfer(0x6200, &rx1);
        if (res != 0xFFFF) {
            if ((rx1 & 0xFFF0) == 0x7200) {
                if (out_rx_word) *out_rx_word = rx1;
                return MB_DETECT_BIOS;
            }
            if ((rx1 & 0xF000) == 0x2000) {
                if (out_rx_word) *out_rx_word = rx1;
                return MB_DETECT_CLIENT;
            }
        }
        mb_delay_us(60);
    }
    return MB_DETECT_NONE;
}

bool multiboot_send_payload(const u8* payload, u32 payload_size) {
    if (!payload || payload_size < 448 || payload_size > 262144 || (payload_size % 16) != 0) {
        return false;
    }

    for (int attempt = 0; attempt < 3; attempt++) {
        MultiBootParam param;
        u8* p_raw = (u8*)&param;
        for (int i = 0; i < (int)sizeof(param); i++) {
            p_raw[i] = 0;
        }

        param.client_data[0] = 0xFF;
        param.client_data[1] = 0xFF;
        param.client_data[2] = 0xFF;
        param.palette_data = PALETTE_DATA;
        param.client_bit = 0;
        param.boot_srcp = payload + 0xC0;
        param.boot_endp = payload + payload_size;

        /* Initialize SIO to 115200 baud Multi-Player mode */
        REG_RCNT = 0x0000;
        REG_SIOCNT = 0x2003;
        mb_delay_us(200);

        /* 1. Detect Clients (Send 0x6200 repeatedly up to 16 times) */
        bool detected = false;
        for (int t = 0; t < 16; t++) {
            u16 rx1 = 0;
            mb_transfer(0x6200, &rx1);
            if ((rx1 & 0xFFF0) == 0x7200) {
                u8 client_id = rx1 & 0xF;
                if (client_id == 2) { /* Slave 1 */
                    param.client_bit |= 2;
                    detected = true;
                    break;
                }
            }
            mb_delay_us(80);
        }
        if (!detected) continue;

        /* 2. Confirm Clients (Send 0x6100 | client_bit) */
        u16 rx_confirm = 0;
        mb_transfer(0x6100 | param.client_bit, &rx_confirm);
        if ((rx_confirm & 0xFFF0) != 0x7200) {
            continue;
        }

        /* 3. Send Header (192 bytes = 96 words) in 16-bit little-endian */
        const u16* header_words = (const u16*)payload;
        bool header_ok = true;
        for (int i = 0x60; i > 0; i--) {
            u16 rx_hdr = 0;
            mb_transfer(*header_words++, &rx_hdr);
            /* Slave replies with (remaining << 8) | client_id */
            if ((rx_hdr & 0xFF00) != (i << 8)) {
                header_ok = false;
                break;
            }
        }
        if (!header_ok) continue;

        /* 4. Exchange master/slave info again */
        u16 rx_end = 0;
        mb_transfer(0x6200, &rx_end);
        if ((rx_end & 0x000F) != (param.client_bit & 0x0F)) {
            continue;
        }

        mb_transfer(0x6200 | param.client_bit, &rx_end);
        if ((rx_end & 0xFFF0) != 0x7200) {
            continue;
        }

        /* 5. Send Palette & Receive Client Data (Send 0x6300 | palette_data) */
        bool got_palette = false;
        for (int t = 0; t < 16; t++) {
            u16 rx_pal = 0;
            mb_transfer(0x6300 | PALETTE_DATA, &rx_pal);
            if ((rx_pal & 0xFF00) == 0x7300) {
                param.client_data[0] = rx_pal & 0xFF;
                got_palette = true;
                break;
            }
            mb_delay_us(80);
        }
        if (!got_palette) continue;

        /* 6. Calculate Handshake Data & Confirm */
        param.handshake_data = (0x11 + param.client_data[0] + param.client_data[1] + param.client_data[2]) & 0xFF;
        u16 rx_hs = 0;
        mb_transfer(0x6400 | param.handshake_data, &rx_hs);
        if ((rx_hs & 0xFF00) != 0x7300) {
            continue;
        }

        /* Brief delay (~500 us) before executing BIOS SWI 0x25 */
        mb_delay_us(500);

        /* 8. Call BIOS SWI 0x25 to complete the main payload transfer & CRC */
        int swi_res = swi_multiboot_call(&param);

        /* Reset SIO cleanly */
        REG_RCNT = 0x0000;
        REG_SIOCNT = 0x2003;
        REG_SIOMLT_SEND = 0x0000;

        if (swi_res == 0) {
            return true;
        }
    }

    return false;
}
