#include "../include/gba.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/sound.h"
#include "../include/sysinfo.h"
#include "../include/display_test.h"
#include "../include/benchmark.h"
#include "../include/help_guide.h"
#include "../include/stress_test.h"
#include "../include/health_audit.h"
#include "../include/localization.h"

enum MenuSelection {
    MENU_HEALTH = 0,
    MENU_SYSINFO,
    MENU_GAMEPAD,
    MENU_DISPLAY,
    MENU_AUDIO,
    MENU_BENCHMARK,
    MENU_STRESS,
    MENU_LINK_SYNC,
    MENU_COUNT
};

static const char* menu_titles[LANG_COUNT][MENU_COUNT] = {
    /* LANG_DE */
    {
        "1. Hardware-Zustand & Audit-Score",
        "2. System & Konsolen-Diagnose",
        "3. Tasten & Gamepad-Hardware",
        "4. Bildschirm & IPS/OLED-Test",
        "5. Sound & Audio-Hardware",
        "6. Speicher & CPU-Benchmarks",
        "7. Spannungs- & Belastungstest",
        "8. Master-Link Sync & SIO-Monitor"
    },
    /* LANG_EN */
    {
        "1. Hardware Health & Audit Score",
        "2. System & Console Diagnostics",
        "3. Gamepad & Button Hardware Check",
        "4. Display Quality & IPS/OLED",
        "5. Sound & Audio Hardware Suite",
        "6. Memory & CPU Benchmarks",
        "7. Power-Rail & Stress Test",
        "8. Master Link Sync & SIO Monitor"
    }
};

static const char* menu_descriptions[LANG_COUNT][MENU_COUNT] = {
    /* LANG_DE */
    {
        "1-Screen Hardware-Pruefbericht\nZustandsnote & Gesamtzertifikat",
        "Modell-Erkennung, WAITCNT-Bus-Timing\nEWRAM-Ausfuehrung & MultiBoot",
        "Echtzeit-Tastenanzeige & Zaehler\nDiagonalen & Pivot-Kurzschluss-Check",
        "Pixelfehler, RGB-Verlaeufe, Schlieren\n59.73Hz Takt, Tearing & 30Hz-Schatten",
        "DMG 1-4 Synth, DirectSound 8-Bit PCM\nStereotrennung & chromatische Skala",
        "EWRAM/IWRAM-Speed, Bus-Integritaet\nCPU-ALU-Leistung & Taktfrequenz",
        "100% 3D-Last & 4-Kanal-Audio-Test\nSpannungsabfall & Elko-Pruefung",
        "Echtzeit-SIO-Verbindung zum Master\nLive-Datenpaket-Austausch & Sync"
    },
    /* LANG_EN */
    {
        "1-screen automated audit sweep\nComponent health grade & pass/fail",
        "Model ID, WAITCNT bus timings\nEWRAM execution & MultiBoot session",
        "Live pad layout, press counters\nDiagonals & pivot error check",
        "Dead pixel, RGB ramps, ghosting\n59.73Hz timing, tearing & 30Hz shadow",
        "DMG 1-4, DirectSound 8-bit PCM\nStereo separation & chromatic scale",
        "EWRAM/IWRAM speed, RAM bus integrity\nCPU ALU test, frame oscillator",
        "100% 3D + 4-channel audio load\nVoltage-drop monitor & recap check",
        "Real-time SIO communication\nLive packet exchange & baud monitor"
    }
};

static const char* card1_titles[LANG_COUNT] = {
    "DIAGNOSE-MODULE (EWRAM)",
    "DIAGNOSTIC MODULES (EWRAM)"
};

static const char* card2_titles[LANG_COUNT] = {
    "MODUL-DETAILS",
    "MODULE DETAILS"
};

static const char* footer_hints[LANG_COUNT] = {
    "A:Start | L/R:DE/EN | SELECT:Hilfe",
    "A:Start | L/R:EN/DE | SELECT:Guide"
};

static const char* const slave_headers[LANG_COUNT] = {
    "LINK-SYNC & SIO-MONITOR",
    "LINK SYNC & SIO MONITOR"
};

static const char* const slave_footers[LANG_COUNT] = {
    "SELECT: Hilfe | B: Hauptmenue",
    "SELECT: Guide | B: Main Menu"
};

static const char* const slave_card1_titles[LANG_COUNT] = {
    "MASTER-LINK PROTOKOLL",
    "MASTER LINK PROTOCOL"
};

static const char* const slave_card2_titles[LANG_COUNT] = {
    "ECHTZEIT-TASTENMATRIX",
    "REAL-TIME BUTTON MATRIX"
};

static void draw_menu_row(int idx, bool is_selected) {
    int y = 28 + idx * 10;
    u16 bg_col = is_selected ? COLOR_CARD_BORDER : COLOR_CARD_BG;
    u16 fg_col = is_selected ? COLOR_WHITE : COLOR_LIGHT_GRAY;

    gfx_draw_rect(6, y - 1, 228, 10, bg_col);
    if (is_selected) {
        gfx_draw_text(10, y + 1, ">", COLOR_CYAN, bg_col, false);
    }
    gfx_draw_text(18, y + 1, menu_titles[g_lang][idx], fg_col, bg_col, false);
}

static void draw_description(int idx) {
    gfx_draw_rect(8, 122, 224, 22, COLOR_CARD_BG);
    gfx_draw_text(8, 122, menu_descriptions[g_lang][idx], COLOR_WHITE, COLOR_CARD_BG, false);
}

/* Dedicated Slave SIO Monitor screen */
static void run_slave_link_monitor(void) {
    gfx_clear(COLOR_NAVY);
    gfx_draw_header(slave_headers[g_lang], "SLAVE 1");
    gfx_draw_footer(slave_footers[g_lang]);
    gfx_draw_card(4, 18, 232, 60, slave_card1_titles[g_lang], COLOR_CARD_BORDER);
    gfx_draw_card(4, 82, 232, 54, slave_card2_titles[g_lang], COLOR_CARD_BORDER);

    int tick = 0;
    int partner_timeout = 0;
    u16 last_rx0 = 0xFFFF;
    u32 packet_count = 0;
    int prev_connected = -1;
    u32 prev_pkt = 0xFFFFFFFF;
    u16 prev_keys = 0xFFFF;
    Language prev_lang = g_lang;

    REG_RCNT = 0;
    REG_SIOCNT = 0x2003; /* Multi-Player Slave Mode, 115200 bps */
    REG_SIOMLT_SEND = 0x2000;

    while (1) {
        vsync();
        input_poll();
        tick++;

        if (key_hit(KEY_B)) {
            break;
        }

        if (key_hit(KEY_SELECT)) {
            run_help_guide(HELP_TOPIC_LINK);
            gfx_clear(COLOR_NAVY);
            gfx_draw_header(slave_headers[g_lang], "SLAVE 1");
            gfx_draw_footer(slave_footers[g_lang]);
            gfx_draw_card(4, 18, 232, 60, slave_card1_titles[g_lang], COLOR_CARD_BORDER);
            gfx_draw_card(4, 82, 232, 54, slave_card2_titles[g_lang], COLOR_CARD_BORDER);
            prev_connected = -1;
            prev_pkt = 0xFFFFFFFF;
            prev_keys = 0xFFFF;
            prev_lang = g_lang;
            continue;
        }

        if (g_lang != prev_lang) {
            prev_lang = g_lang;
            gfx_clear(COLOR_NAVY);
            gfx_draw_header(slave_headers[g_lang], "SLAVE 1");
            gfx_draw_footer(slave_footers[g_lang]);
            gfx_draw_card(4, 18, 232, 60, slave_card1_titles[g_lang], COLOR_CARD_BORDER);
            gfx_draw_card(4, 82, 232, 54, slave_card2_titles[g_lang], COLOR_CARD_BORDER);
            prev_connected = -1;
            prev_pkt = 0xFFFFFFFF;
            prev_keys = 0xFFFF;
        }

        u16 keys = g_input.current & KEY_ANY;
        REG_SIOMLT_SEND = (u16)(0x2000 | keys);

        if (!(REG_SIOCNT & 0x0080)) {
            if (REG_SIOCNT & 0x0040) {
                REG_SIOCNT = 0x2003;
            } else {
                int hw_id = (REG_SIOCNT >> 4) & 3;
                u16 rx0 = REG_SIOMULTI0;
                if (hw_id >= 1 && rx0 != 0xFFFF && ((rx0 & 0xF000) == 0x1000 || rx0 == 0x6200)) {
                    partner_timeout = 60;
                    if (rx0 != last_rx0) {
                        last_rx0 = rx0;
                        packet_count++;
                    }
                }
            }
        }

        bool connected = (partner_timeout > 0);
        if (partner_timeout > 0) {
            partner_timeout--;
        }

        if ((int)connected != prev_connected) {
            prev_connected = connected;
            gfx_draw_rect(8, 30, 224, 44, COLOR_CARD_BG);
            if (connected) {
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 32, "STATUS: VERBUNDEN MIT MASTER (P1)", COLOR_EMERALD, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 44, "ROLLE : SPIELER 2 (SLAVE 1) [GRAU]", COLOR_CYAN, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 56, "BUS   : 115200 BPS MULTI-PLAYER", COLOR_GOLD, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text(8, 32, "STATUS: CONNECTED TO MASTER (P1)", COLOR_EMERALD, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 44, "ROLE  : PLAYER 2 (SLAVE 1) [GREY]", COLOR_CYAN, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 56, "BUS   : 115200 BPS MULTI-PLAYER", COLOR_GOLD, COLOR_CARD_BG, false);
                }
            } else {
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 32, "STATUS: WARTE AUF MASTER-TAKT...", COLOR_AMBER, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 44, "ROLLE : SPIELER 2 (STANDBY)", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 56, "KABEL : Lila Stecker in GBA 1!", COLOR_DARK_GRAY, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text(8, 32, "STATUS: WAITING FOR MASTER CLOCK...", COLOR_AMBER, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 44, "ROLE  : PLAYER 2 (STANDBY)", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 56, "CABLE : Purple plug in GBA 1!", COLOR_DARK_GRAY, COLOR_CARD_BG, false);
                }
            }
        }

        if (packet_count != prev_pkt || (tick & 15) == 0) {
            prev_pkt = packet_count;
            gfx_draw_rect(8, 66, 224, 10, COLOR_CARD_BG);
            if (g_lang == LANG_DE) {
                gfx_draw_text_fmt(8, 66, COLOR_CYAN, COLOR_CARD_BG, false,
                                  "Pakete: %5u | Letztes RX: 0x%04X", packet_count, last_rx0);
            } else {
                gfx_draw_text_fmt(8, 66, COLOR_CYAN, COLOR_CARD_BG, false,
                                  "Packets: %5u | Last RX: 0x%04X", packet_count, last_rx0);
            }
        }

        /* Draw live button states ONLY when keys change (zero flicker) */
        if (keys != prev_keys) {
            prev_keys = keys;
            gfx_draw_rect(8, 96, 224, 36, COLOR_CARD_BG);
            gfx_draw_text_fmt(10, 98, (keys & KEY_A) ? COLOR_AMBER : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false, "[A]");
            gfx_draw_text_fmt(34, 98, (keys & KEY_B) ? COLOR_AMBER : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false, "[B]");
            gfx_draw_text_fmt(58, 98, (keys & KEY_SELECT) ? COLOR_CYAN : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false, "[SEL]");
            gfx_draw_text_fmt(92, 98, (keys & KEY_START) ? COLOR_CYAN : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false, "[STA]");
            gfx_draw_text_fmt(130, 98, (keys & KEY_L) ? COLOR_GOLD : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false, "[L]");
            gfx_draw_text_fmt(154, 98, (keys & KEY_R) ? COLOR_GOLD : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false, "[R]");
            gfx_draw_text_fmt(10, 114, (keys & KEY_UP) ? COLOR_EMERALD : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false, "[UP]");
            gfx_draw_text_fmt(40, 114, (keys & KEY_DOWN) ? COLOR_EMERALD : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false, "[DN]");
            gfx_draw_text_fmt(70, 114, (keys & KEY_LEFT) ? COLOR_EMERALD : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false, "[LF]");
            gfx_draw_text_fmt(100, 114, (keys & KEY_RIGHT) ? COLOR_EMERALD : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false, "[RT]");
            if (g_lang == LANG_DE) {
                gfx_draw_text(130, 114, "Tasten an P1", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
            } else {
                gfx_draw_text(130, 114, "Keys sent to P1", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
            }
        }
    }
}

void mb_main(void) {
    /* Initialize GBA hardware subsystems */
    gfx_init();
    input_init();
    sound_init();

    /* Configure Link Port for background Multi-Player Slave Mode (115200 baud) */
    REG_RCNT = 0;
    REG_SIOCNT = 0x2003;
    REG_SIOMLT_SEND = 0x2000;

    int selected = 0;
    int prev_selected = -1;
    bool need_full_redraw = true;
    int tick = 0;

    while (1) {
        vsync();
        input_poll();
        tick++;

        /* Keep send register primed with current key state in background */
        REG_SIOMLT_SEND = (u16)(0x2000 | (g_input.current & KEY_ANY));

        /* 1. Full Redraw when entering or returning from module */
        if (need_full_redraw) {
            gfx_clear(COLOR_NAVY);
            gfx_draw_header("GBA HARDWARE CHECK", NULL);
            gfx_draw_text(126, 3, lang_get_badge(), COLOR_GOLD, COLOR_HEADER_BG, false);
            gfx_draw_header_chaser(tick, "EWRAM", false, true);

            gfx_draw_card(4, 16, 232, 94, card1_titles[g_lang], COLOR_CARD_BORDER);
            for (int i = 0; i < MENU_COUNT; i++) {
                draw_menu_row(i, i == selected);
            }

            gfx_draw_card(4, 112, 232, 34, card2_titles[g_lang], COLOR_CARD_BORDER);
            draw_description(selected);

            gfx_draw_footer(footer_hints[g_lang]);

            prev_selected = selected;
            need_full_redraw = false;
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

        /* 3. Dirty row update on selection change */
        if (selected != prev_selected) {
            draw_menu_row(prev_selected, false);
            draw_menu_row(selected, true);
            draw_description(selected);
            prev_selected = selected;
        }

        /* 4. Help & Reference Guide on SELECT */
        if (key_hit(KEY_SELECT)) {
            sound_play_square2(1750, 1, PAN_CENTER);
            enum HelpTopic topic = (selected == MENU_LINK_SYNC) ? HELP_TOPIC_LINK : (enum HelpTopic)selected;
            run_help_guide(topic);
            sound_init();
            REG_RCNT = 0;
            REG_SIOCNT = 0x2003;
            need_full_redraw = true;
            continue;
        }

        /* 5. START Button: Quick Health Audit Shortcut */
        if (key_hit(KEY_START)) {
            sound_play_square1(1750, 2, 2, true, PAN_CENTER);
            run_health_audit_screen();
            sound_init();
            REG_RCNT = 0;
            REG_SIOCNT = 0x2003;
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
                case MENU_LINK_SYNC:
                    run_slave_link_monitor();
                    break;
            }
            sound_init();
            REG_RCNT = 0;
            REG_SIOCNT = 0x2003;
            need_full_redraw = true;
            continue;
        }

        /* 6. Oldschool diagnostic LED chaser animation */
        gfx_draw_header_chaser(tick, "EWRAM", false, false);
    }
}
