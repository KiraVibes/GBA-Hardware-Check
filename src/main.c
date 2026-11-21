#include "../include/gba.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/sound.h"
#include "../include/rtc.h"
#include "../include/sysinfo.h"
#include "../include/display_test.h"
#include "../include/benchmark.h"
#include "../include/help_guide.h"
#include "../include/stress_test.h"
#include "../include/link_test.h"
#include "../include/ezflash.h"
#include "../include/health_audit.h"
#include "../include/battery_test.h"
#include "../include/localization.h"

enum MenuSelection {
    MENU_HEALTH = 0,
    MENU_SYSINFO,
    MENU_GAMEPAD,
    MENU_DISPLAY,
    MENU_AUDIO,
    MENU_BENCHMARK,
    MENU_STRESS,
    MENU_BATTERY,
    MENU_LINK,
    MENU_EZFLASH,
    MENU_COUNT
};

#define VISIBLE_ROWS 8

static const char* menu_titles[LANG_COUNT][MENU_COUNT] = {
    /* LANG_DE */
    {
        "1. Hardware-Zustand & Audit-Score",
        "2. System & Konsolen-Diagnose",
        "3. Tasten & Gamepad-Hardware",
        "4. Bildschirm & IPS/OLED-Test",
        "5. Sound, DirectSound & MIDI-Test",
        "6. Speicher & CPU-Benchmarks",
        "7. Spannungs- & Belastungstest",
        "8. Akku-Laufzeit & Entlade-Timer",
        "9. Link-Kabel & SIO-Port-Test",
        "10. EZ-Flash Omega Modul-Suite"
    },
    /* LANG_EN */
    {
        "1. Hardware Health & Audit Score",
        "2. System & Console Diagnostics",
        "3. Gamepad & Button Hardware Check",
        "4. Display Quality & IPS/OLED",
        "5. Sound, DirectSound & MIDI Suite",
        "6. Memory & CPU Benchmarks",
        "7. Power-Rail & Stress Test",
        "8. Battery Endurance & Run Timer",
        "9. Link Cable & Serial Port Check",
        "10. EZ-Flash Omega Suite"
    }
};

static const char* menu_descriptions[LANG_COUNT][MENU_COUNT] = {
    /* LANG_DE */
    {
        "1-Screen Hardware-Pruefbericht\nZustandsnote & Gesamtzertifikat",
        "Modell-Erkennung, WAITCNT-Bus-Timing\nSRAM-Speicher & Live-RTC-Uhr",
        "Echtzeit-Tastenanzeige & Zaehler\nDiagonalen & Pivot-Kurzschluss-Check",
        "Pixelfehler, RGB-Verlaeufe, Schlieren\n59.73Hz Takt, Tearing & 30Hz-Schatten",
        "DMG 1-4 Synth, DirectSound 8-Bit PCM\nMIDI-Player & Stereokanaele",
        "EWRAM/ROM-Speed, Bus-Integritaet\nCPU-ALU-Leistung & Taktfrequenz",
        "100% 3D-Last & 4-Kanal-Audio-Test\nSpannungsabfall & Elko-Pruefung",
        "Kapazitaetstest & Entladedauer\nSRAM-Speicherung bei Abschaltung",
        "SIO-Port & Player-1-Master-Check\nLila vs Grauer Stecker & Pin-Logik",
        "Haptischer Rumble-Motor-Stufentest\nZweifarbige Modul-Status-LED"
    },
    /* LANG_EN */
    {
        "1-screen automated audit sweep\nComponent health grade & pass/fail",
        "Model ID, WAITCNT bus timings\nSave SRAM & EZ-Flash live RTC clock",
        "Live pad layout, press counters\nDiagonals & pivot error check",
        "Dead pixel, RGB ramps, ghosting\n59.73Hz timing, tearing & 30Hz shadow",
        "DMG 1-4, DirectSound 8-bit PCM\nMIDI Sequencer with sound profiles",
        "EWRAM/ROM speed, RAM bus integrity\nCPU ALU test, frame oscillator",
        "100% 3D + 4-channel audio load\nVoltage-drop monitor & recap check",
        "Battery runtime & endurance test\nSaves state to SRAM on depletion",
        "SIO port check & Master Player 1\nPurple vs grey plug & pin logic",
        "Haptic rumble motor power levels\nDual-color cartridge status LED"
    }
};

static const char* card1_titles[LANG_COUNT] = {
    "DIAGNOSE-MODULE",
    "DIAGNOSTIC MODULES"
};

static const char* card2_titles[LANG_COUNT] = {
    "MODUL-DETAILS",
    "MODULE DETAILS"
};

static const char* footer_hints[LANG_COUNT] = {
    "A:Start | L/R:DE/EN | SELECT:Hilfe",
    "A:Start | L/R:EN/DE | SELECT:Guide"
};

static void draw_menu_row(int slot, int idx, bool is_selected) {
    int y = 28 + slot * 10;
    u16 bg_col = is_selected ? COLOR_CARD_BORDER : COLOR_CARD_BG;
    u16 fg_col = is_selected ? COLOR_WHITE : COLOR_LIGHT_GRAY;

    gfx_draw_rect(6, y - 1, 228, 10, bg_col);
    if (is_selected) {
        gfx_draw_text(10, y + 1, ">", COLOR_CYAN, bg_col, false);
    }
    gfx_draw_text(18, y + 1, menu_titles[g_lang][idx], fg_col, bg_col, false);
}

static void draw_visible_menu(int selected, int scroll_top) {
    for (int slot = 0; slot < VISIBLE_ROWS; slot++) {
        int item_idx = scroll_top + slot;
        if (item_idx < MENU_COUNT) {
            draw_menu_row(slot, item_idx, item_idx == selected);
        }
    }

    /* Scroll Indicators */
    if (scroll_top > 0) {
        gfx_draw_text(228, 18, "^", COLOR_CYAN, COLOR_CARD_BORDER, true);
    } else {
        gfx_draw_rect(226, 18, 8, 8, COLOR_CARD_BORDER);
    }
    if (scroll_top + VISIBLE_ROWS < MENU_COUNT) {
        gfx_draw_text(228, 98, "v", COLOR_CYAN, COLOR_CARD_BG, false);
    } else {
        gfx_draw_rect(226, 98, 8, 8, COLOR_CARD_BG);
    }
}

static void draw_description(int idx) {
    gfx_draw_rect(8, 123, 224, 21, COLOR_CARD_BG);
    gfx_draw_text(8, 123, menu_descriptions[g_lang][idx], COLOR_WHITE, COLOR_CARD_BG, false);
}

int main(void) {
    /* Initialize GBA hardware subsystems */
    gfx_init();
    input_init();
    sound_init();

    int selected = 0;
    int prev_selected = -1;
    int scroll_top = 0;
    int prev_scroll_top = -1;
    bool need_full_redraw = true;
    RtcTime rtc;
    int prev_rtc_sec = -1;
    int tick = 0;

    while (1) {
        vsync();
        input_poll();
        tick++;

        /* 1. Full Redraw when entering or returning from module */
        if (need_full_redraw) {
            gfx_clear(COLOR_NAVY);
            gfx_draw_header("GBA HARDWARE CHECK", NULL);
            gfx_draw_text(126, 3, lang_get_badge(), COLOR_GOLD, COLOR_HEADER_BG, false);
            gfx_draw_header_chaser(tick, "READY", true, true);

            gfx_draw_card(4, 16, 232, 94, card1_titles[g_lang], COLOR_CARD_BORDER);
            draw_visible_menu(selected, scroll_top);

            gfx_draw_card(4, 112, 232, 34, card2_titles[g_lang], COLOR_CARD_BORDER);
            draw_description(selected);

            gfx_draw_footer(footer_hints[g_lang]);

            prev_selected = selected;
            prev_scroll_top = scroll_top;
            need_full_redraw = false;
            prev_rtc_sec = -1;
        }

        /* 2. Navigation */
        if (key_hit(KEY_UP)) {
            selected = (selected - 1 + MENU_COUNT) % MENU_COUNT;
            sound_play_square2(1800, 1, PAN_CENTER);
        }
        if (key_hit(KEY_DOWN)) {
            selected = (selected + 1) % MENU_COUNT;
            sound_play_square2(1700, 1, PAN_CENTER);
        }

        /* Language Toggle with L and R triggers */
        if (key_hit(KEY_L) || key_hit(KEY_R)) {
            sound_play_square2(2000, 1, PAN_CENTER);
            lang_toggle();
            need_full_redraw = true;
            continue;
        }

        /* Update scroll window */
        if (selected < scroll_top) {
            scroll_top = selected;
        } else if (selected >= scroll_top + VISIBLE_ROWS) {
            scroll_top = selected - VISIBLE_ROWS + 1;
        }

        /* 3. Redraw rows on selection or scroll change */
        if (scroll_top != prev_scroll_top) {
            draw_visible_menu(selected, scroll_top);
            draw_description(selected);
            prev_selected = selected;
            prev_scroll_top = scroll_top;
        } else if (selected != prev_selected) {
            draw_menu_row(prev_selected - scroll_top, prev_selected, false);
            draw_menu_row(selected - scroll_top, selected, true);
            draw_description(selected);
            prev_selected = selected;
        }

        /* 4. Help & Reference Guide on SELECT */
        if (key_hit(KEY_SELECT)) {
            sound_play_square2(1750, 1, PAN_CENTER);
            enum HelpTopic topic = (selected < HELP_TOPIC_COUNT) ? (enum HelpTopic)selected : HELP_TOPIC_HEALTH;
            run_help_guide(topic);
            need_full_redraw = true;
            continue;
        }

        /* 5. START Button: Quick Health Audit Shortcut */
        if (key_hit(KEY_START)) {
            sound_play_square1(1750, 2, 2, true, PAN_CENTER);
            run_health_audit_screen();
            sound_init();
            need_full_redraw = true;
            continue;
        }

        /* 6. A Button: Selection Action */
        if (key_hit(KEY_A)) {
            sound_play_square1(1750, 2, 2, true, PAN_CENTER);
            switch (selected) {
                case MENU_HEALTH:
                    run_health_audit_screen();
                    break;
                case MENU_SYSINFO:
                    run_sysinfo_screen();
                    break;
                case MENU_GAMEPAD:
                    run_input_test();
                    break;
                case MENU_DISPLAY:
                    run_display_test();
                    break;
                case MENU_AUDIO:
                    run_audio_test();
                    break;
                case MENU_BENCHMARK:
                    run_benchmark_screen();
                    break;
                case MENU_STRESS:
                    run_stress_test();
                    break;
                case MENU_BATTERY:
                    run_battery_test();
                    break;
                case MENU_LINK:
                    run_link_test();
                    break;
                case MENU_EZFLASH:
                    run_ezflash_screen();
                    break;
            }
            sound_init();
            need_full_redraw = true;
            continue;
        }

        /* 7. Oldschool diagnostic LED chaser animation */
        gfx_draw_header_chaser(tick, "READY", true, false);

        /* 8. Live RTC clock update (only when seconds change) */
        if ((tick & 15) == 0) {
            bool has_rtc = rtc_read_time(&rtc);
            if (has_rtc && rtc.present && rtc.seconds != prev_rtc_sec) {
                prev_rtc_sec = rtc.seconds;
                gfx_draw_text_fmt(4, SCREEN_HEIGHT - 10, COLOR_GOLD, COLOR_HEADER_BG, false,
                                  (g_lang == LANG_DE) ? "RTC: %02d:%02d:%02d | L/R:DE/EN | HILFE"
                                                      : "RTC: %02d:%02d:%02d | L/R:EN/DE | GUIDE",
                                  rtc.hours, rtc.minutes, rtc.seconds);
            }
        }
    }

    return 0;
}
