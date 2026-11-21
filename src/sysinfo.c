#include "../include/sysinfo.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/rtc.h"
#include "../include/help_guide.h"

#ifndef STANDALONE_MULTIBOOT
static ConsoleModel detect_model(void) {
    /* Test 1: Check REG_WRAMCNT (0x04000800)
       On genuine GBA/GBA SP, REG_WRAMCNT can be written with waitstate bits.
       On Nintendo DS (in GBA mode), this register is NOT implemented and will not retain writes.
    */
    vu8* wramcnt = (vu8*)0x04000800;
    u8 old_val = *wramcnt;
    *wramcnt = 0x03;
    u8 test_val = *wramcnt;
    *wramcnt = old_val;

    if (test_val != 0x03) {
        return MODEL_NINTENDO_DS;
    }

    return MODEL_GBA_CLASSIC;
}
#endif

static const u8 sram_cycles[] = { 4, 3, 2, 8 };
static const u8 rom_ws_first[] = { 4, 3, 2, 8 };
static const u8 rom_ws_second[] = { 2, 1 };

void sysinfo_gather(SystemInfo* info) {
#ifdef STANDALONE_MULTIBOOT
    /* When executing code from EWRAM (0x02000000), REG_WRAMCNT (0x04000800) must
       NEVER be written to, as Bit 0 is EWRAM Disable and crashes the GBA instantly! */
    info->model = MODEL_GBA_CLASSIC;
    info->model_name = "Game Boy Advance / SP / Micro";
#else
    info->model = detect_model();
    if (info->model == MODEL_NINTENDO_DS) {
        info->model_name = "Nintendo DS (Slot-2 GBA Mode)";
    } else {
        info->model_name = "Game Boy Advance / SP / Micro";
    }
#endif

    info->waitcnt = REG_WAITCNT;
    info->sram_waitstates = sram_cycles[info->waitcnt & 0x03];
    info->rom_ws0_first = rom_ws_first[(info->waitcnt >> 2) & 0x03];
    info->rom_ws0_second = rom_ws_second[(info->waitcnt >> 4) & 0x01];
    info->prefetch_enabled = (info->waitcnt & (1 << 14)) != 0;

    /* Read ROM / MultiBoot Header */
#ifdef STANDALONE_MULTIBOOT
    const char* rom_hdr = (const char*)0x020000A0;
#else
    const char* rom_hdr = (const char*)0x080000A0;
#endif
    for (int i = 0; i < 12; i++) {
        info->rom_title[i] = rom_hdr[i] ? rom_hdr[i] : ' ';
    }
    info->rom_title[12] = '\0';

    for (int i = 0; i < 4; i++) {
        info->rom_code[i] = rom_hdr[12 + i] ? rom_hdr[12 + i] : ' ';
    }
    info->rom_code[4] = '\0';

    info->maker_code[0] = rom_hdr[16];
    info->maker_code[1] = rom_hdr[17];
    info->maker_code[2] = '\0';

#ifndef STANDALONE_MULTIBOOT
    /* Non-destructive SRAM / FRAM check at topmost unused byte (0x0E007FFF) */
    vu8* sram_ptr = (vu8*)(MEM_SRAM + 0x7FFF);
    u8 orig = *sram_ptr;
    u8 inv = (u8)(~orig);
    *sram_ptr = inv;
    u8 check = *sram_ptr;
    *sram_ptr = orig;

    info->sram_detected = (check == inv);
    info->rtc_detected = rtc_probe();
#else
    info->sram_detected = false;
    info->rtc_detected = false;
#endif
}

void run_sysinfo_screen(void) {
    SystemInfo info;
    sysinfo_gather(&info);

    /* 1. Draw static screen components ONCE (flicker-free) */
    bool need_redraw = true;
#ifndef STANDALONE_MULTIBOOT
    RtcTime rtc;
    int prev_sec = -1;
#endif

    while (1) {
        if (need_redraw) {
            gfx_clear(COLOR_NAVY);
            gfx_draw_header("SYSTEM DIAGNOSTICS", "INFO");
            gfx_draw_footer("SELECT: Info Guide | B: Return to Menu");

            /* Card 1: Console & CPU Info */
            gfx_draw_card(4, 18, 232, 42, "CONSOLE & PROCESSOR", COLOR_CARD_BORDER);
            gfx_draw_text(8, 30, "Model:", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
            gfx_draw_text(48, 30, info.model_name, (info.model == MODEL_NINTENDO_DS) ? COLOR_AMBER : COLOR_EMERALD, COLOR_CARD_BG, false);

            gfx_draw_text(8, 42, "CPU:", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
            gfx_draw_text(48, 42, "ARM7TDMI @ 16.78 MHz (3-stage)", COLOR_WHITE, COLOR_CARD_BG, false);

            /* Card 2: Memory & Bus Timings */
            gfx_draw_card(4, 62, 232, 40, "BUS TIMINGS & WAITSTATES", COLOR_CARD_BORDER);
            gfx_draw_text_fmt(8, 74, COLOR_WHITE, COLOR_CARD_BG, false,
                              "ROM WS0: %d,%d cyc | SRAM: %d cyc",
                              info.rom_ws0_first, info.rom_ws0_second, info.sram_waitstates);
            gfx_draw_text_fmt(8, 86, COLOR_WHITE, COLOR_CARD_BG, false,
                              "Prefetch: [%s] | WAITCNT: 0x%04X",
                              info.prefetch_enabled ? "ACTIVE" : "OFF", info.waitcnt);

            /* Card 3: Cartridge & EZ-Flash Omega Status */
#ifdef STANDALONE_MULTIBOOT
            gfx_draw_card(4, 104, 232, 42, "MULTIBOOT EWRAM STATUS", COLOR_CARD_BORDER);
            gfx_draw_text(8, 116, "Payload: EWRAM (0x02000000)", COLOR_EMERALD, COLOR_CARD_BG, false);
            gfx_draw_text(8, 128, "Cart: NONE (MULTIBOOT ACTIVE)", COLOR_CYAN, COLOR_CARD_BG, false);
#else
            gfx_draw_card(4, 104, 232, 42, "CARTRIDGE & EZ-FLASH RTC", COLOR_CARD_BORDER);
            gfx_draw_text_fmt(8, 116, COLOR_WHITE, COLOR_CARD_BG, false,
                              "SRAM: [%s] | ROM: %s (%.6s)",
                              info.sram_detected ? "OK" : "OFF", info.rom_code, info.rom_title);
#endif

#ifndef STANDALONE_MULTIBOOT
            prev_sec = -1;
#endif
            need_redraw = false;
        }

        vsync();
        input_poll();

        if (key_hit(KEY_B)) {
            break;
        }

        if (key_hit(KEY_SELECT)) {
            run_help_guide(HELP_TOPIC_SYSTEM);
            need_redraw = true;
            continue;
        }

#ifndef STANDALONE_MULTIBOOT
        /* Update live RTC line ONLY when second changes */
        bool has_rtc = rtc_read_time(&rtc);
        if (has_rtc && rtc.present) {
            if (rtc.seconds != prev_sec) {
                prev_sec = rtc.seconds;
                gfx_draw_rect(8, 128, 220, 10, COLOR_CARD_BG);
                gfx_draw_text_fmt(8, 128, COLOR_EMERALD, COLOR_CARD_BG, false,
                                  "RTC LIVE: 20%02d-%02d-%02d %02d:%02d:%02d",
                                  rtc.year, rtc.month, rtc.day,
                                  rtc.hours, rtc.minutes, rtc.seconds);
            }
        } else if (prev_sec == -1) {
            prev_sec = 0;
            gfx_draw_text(8, 128, "RTC: NOT DETECTED (CHECK BATTERY)", COLOR_CORAL, COLOR_CARD_BG, false);
        }
#endif
    }
}
