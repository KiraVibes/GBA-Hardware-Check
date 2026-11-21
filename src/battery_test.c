#include "../include/battery_test.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/sound.h"
#include "../include/rtc.h"
#include "../include/help_guide.h"
#include "../include/localization.h"

#define BATTERY_SRAM_MAGIC  0x47424142 /* "GBAB" */
#define BATTERY_SRAM_OFFSET 0x7000     /* Upper 4KB of 32KB SRAM (avoids MIDI presets at 0x0000) */
#define SRAM_PROBE_OFFSET   0x7FFF     /* Topmost unused SRAM byte for non-destructive probe */

typedef struct {
    u32 magic;          /* 0x47424142 */
    u32 elapsed_secs;   /* Run-time in seconds */
    u8  load_mode;      /* 0: IDLE, 1: NORMAL, 2: MAX STRESS */
    u8  state;          /* 0: STOPPED, 1: RUNNING (ended by power loss) */
    u8  has_rtc;        /* 1 if RTC was active, 0 otherwise */
    u8  rtc_start_h;    /* Start hour (0..23) */
    u8  rtc_start_m;    /* Start minute (0..59) */
    u8  rtc_start_s;    /* Start second (0..59) */
    u8  reserved[2];
    u8  checksum;       /* XOR validation of bytes 0..13 */
    u8  padding;
} BatteryRecord;

static const char* const battery_headers[LANG_COUNT] = {
    "AKKU-LAUFZEIT & ENTLADETEST",
    "BATTERY ENDURANCE BENCHMARK"
};

static const char* const battery_footers[LANG_COUNT] = {
    "A:Start | L/R:Last | B:Ende",
    "A:Start | L/R:Load | B:Back"
};

static const char* const card1_titles[LANG_COUNT] = {
    "LETZTER GESPEICHERTER TEST",
    "LAST SAVED RUNTIME (SRAM)"
};

static const char* const card2_titles[LANG_COUNT] = {
    "NEUER LAUFZEITTEST (AKTIV)",
    "ACTIVE RUN-DOWN BENCHMARK"
};

static const char* const mode_names[LANG_COUNT][3] = {
    /* LANG_DE */
    {
        "1. IDLE (DISPLAY AN)",
        "2. NORMAL (GAMEPLAY)",
        "3. MAX (3D-WUERFEL)"
    },
    /* LANG_EN */
    {
        "1. IDLE (DISPLAY ONLY)",
        "2. NORMAL (GAMEPLAY)",
        "3. MAX (3D CUBE)"
    }
};

/* Fixed-point sine table (64 steps = 360 deg, scaled by 128) */
static const s16 sin_lut[64] = {
      0,  12,  25,  37,  49,  60,  71,  81,
     90,  98, 106, 112, 118, 122, 125, 127,
    128, 127, 125, 122, 118, 112, 106,  98,
     90,  81,  71,  60,  49,  37,  25,  12,
      0, -12, -25, -37, -49, -60, -71, -81,
    -90, -98,-106,-112,-118,-122,-125,-127,
   -128,-127,-125,-122,-118,-112,-106, -98,
    -90, -81, -71, -60, -49, -37, -25, -12
};

static inline s16 f_sin(int angle) { return sin_lut[angle & 63]; }
static inline s16 f_cos(int angle) { return sin_lut[(angle + 16) & 63]; }

/* 3D Cube vertices: scaled to +/-9 so rotation fits inside viewport */
static const s16 cube_v[8][3] = {
    {-9, -9, -9}, { 9, -9, -9}, { 9,  9, -9}, {-9,  9, -9},
    {-9, -9,  9}, { 9, -9,  9}, { 9,  9,  9}, {-9,  9,  9}
};

/* 12 Edges connecting vertices */
static const u8 cube_edges[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0},
    {4,5}, {5,6}, {6,7}, {7,4},
    {0,4}, {1,5}, {2,6}, {3,7}
};

static u8 compute_checksum(const BatteryRecord* rec) {
    u8 sum = 0x5A;
    const u8* ptr = (const u8*)rec;
    for (int i = 0; i < 14; i++) {
        sum ^= ptr[i];
    }
    return sum;
}

static bool is_sram_writable(void) {
    vu8* sram = (vu8*)(MEM_SRAM + SRAM_PROBE_OFFSET);
    u8 orig = *sram;
    u8 inv = (u8)(~orig);
    *sram = inv;
    u8 check = *sram;
    *sram = orig;
    return (check == inv);
}

static bool read_sram_record_at(u32 offset, BatteryRecord* out) {
    vu8* sram = (vu8*)(MEM_SRAM + offset);
    u8* dest = (u8*)out;
    for (int i = 0; i < (int)sizeof(BatteryRecord); i++) {
        dest[i] = sram[i];
    }

    if (out->magic != BATTERY_SRAM_MAGIC) return false;

    if (out->checksum != compute_checksum(out)) {
        /* Check backwards compatibility for 12-byte legacy struct */
        u8 old_sum = 0x5A;
        const u8* ptr = (const u8*)out;
        for (int i = 0; i < 10; i++) old_sum ^= ptr[i];
        if (ptr[10] == old_sum && out->load_mode <= 2) {
            out->has_rtc = 0;
            out->rtc_start_h = 0;
            out->rtc_start_m = 0;
            out->rtc_start_s = 0;
            return true;
        }
        return false;
    }

    if (out->load_mode > 2) return false;
    return true;
}

static bool read_sram_record(BatteryRecord* out) {
    if (!is_sram_writable()) return false;
    if (read_sram_record_at(BATTERY_SRAM_OFFSET, out)) return true;
    /* Legacy fallback if an older build wrote BatteryRecord at 0x0000 */
    return read_sram_record_at(0x0000, out);
}

static void write_sram_record(const BatteryRecord* in) {
    if (!is_sram_writable()) return;

    BatteryRecord copy = *in;
    copy.magic = BATTERY_SRAM_MAGIC;
    copy.checksum = compute_checksum(&copy);

    vu8* sram = (vu8*)(MEM_SRAM + BATTERY_SRAM_OFFSET);
    const u8* src = (const u8*)&copy;
    for (int i = 0; i < (int)sizeof(BatteryRecord); i++) {
        sram[i] = src[i];
    }
}

/* Hardware Crystal Timers (Timer 2 + Timer 3 Cascade) */
static void hw_timer_init(void) {
    REG_TM2CNT_H = 0;
    REG_TM3CNT_H = 0;

    /* TM2: Prescaler 1024 (16.384 kHz). Reload = 65536 - 16384 = 0xC000.
       Overflows exactly once every 16,384 cycles = 1.0000000 second */
    REG_TM2CNT_L = 0xC000;
    REG_TM2CNT_H = 0x0083; /* Start TM2, Prescaler 1024 */

    /* TM3: Cascade mode (increments on TM2 overflow). */
    REG_TM3CNT_L = 0;
    REG_TM3CNT_H = 0x0084; /* Start TM3, Count-up */
}

static void hw_timer_stop(void) {
    REG_TM2CNT_H = 0;
    REG_TM3CNT_H = 0;
}

void run_battery_test(void) {
    bool sram_ok = is_sram_writable();
    BatteryRecord saved_rec;
    bool has_saved = read_sram_record(&saved_rec);

    /* Cartridge RTC check */
    RtcTime rtc;
    bool rtc_available = rtc_read_time(&rtc);
    u8 rtc_start_h = 0, rtc_start_m = 0, rtc_start_s = 0;

    int mode = 2; /* Default: Max Stress with 3D Wireframe Cube */
    bool running = false;
    u32 current_secs = 0;
    u32 prev_display_secs = 0xFFFFFFFF;
    int tick = 0;
    u16 last_tm3 = 0;
    bool need_full_redraw = true;
    Language prev_lang = g_lang;

    /* 3D Wireframe rotation states */
    int angle_x = 0;
    int angle_y = 0;
    static int prev_cube_x[8];
    static int prev_cube_y[8];
    bool cube_rendered = false;

    while (1) {
        vsync();
        input_poll();
        tick++;

        /* 1. Return to Main Menu */
        if (key_hit(KEY_B)) {
            if (running) {
                running = false;
                hw_timer_stop();
                BatteryRecord cur;
                cur.magic = BATTERY_SRAM_MAGIC;
                cur.elapsed_secs = current_secs;
                cur.load_mode = (u8)mode;
                cur.state = 0; /* Stopped by user */
                cur.has_rtc = rtc_available ? 1 : 0;
                cur.rtc_start_h = rtc_start_h;
                cur.rtc_start_m = rtc_start_m;
                cur.rtc_start_s = rtc_start_s;
                cur.reserved[0] = 0;
                cur.reserved[1] = 0;
                cur.padding = 0;
                write_sram_record(&cur);
                sound_stop_all();
            }
            hw_timer_stop();
            break;
        }

        /* 2. Open Help Guide */
        if (key_hit(KEY_SELECT)) {
            sound_play_square2(1750, 1, PAN_CENTER);
            run_help_guide(HELP_TOPIC_BATTERY);
            need_full_redraw = true;
            continue;
        }

        /* 3. Reset Active Timer: START + A */
        if (key_down(KEY_START) && key_hit(KEY_A)) {
            sound_play_square1(1600, 2, 2, true, PAN_CENTER);
            running = false;
            hw_timer_stop();
            current_secs = 0;
            prev_display_secs = 0xFFFFFFFF;
            last_tm3 = 0;
            sound_stop_all();

            BatteryRecord cur;
            cur.magic = BATTERY_SRAM_MAGIC;
            cur.elapsed_secs = 0;
            cur.load_mode = (u8)mode;
            cur.state = 0;
            cur.has_rtc = 0;
            cur.rtc_start_h = 0;
            cur.rtc_start_m = 0;
            cur.rtc_start_s = 0;
            cur.reserved[0] = 0;
            cur.reserved[1] = 0;
            cur.padding = 0;
            write_sram_record(&cur);
            has_saved = read_sram_record(&saved_rec);
            need_full_redraw = true;
            continue;
        }

        /* 4. Toggle Start / Pause: A */
        if (key_hit(KEY_A)) {
            running = !running;
            if (running) {
                hw_timer_init();
                last_tm3 = 0;
                if (rtc_read_time(&rtc)) {
                    rtc_available = true;
                    if (current_secs == 0) {
                        rtc_start_h = rtc.hours;
                        rtc_start_m = rtc.minutes;
                        rtc_start_s = rtc.seconds;
                    }
                } else {
                    rtc_available = false;
                }
                sound_play_square2(2000, 1, PAN_CENTER);
            } else {
                hw_timer_stop();
                sound_play_square2(1500, 1, PAN_CENTER);
                sound_stop_all();
            }

            BatteryRecord cur;
            cur.magic = BATTERY_SRAM_MAGIC;
            cur.elapsed_secs = current_secs;
            cur.load_mode = (u8)mode;
            cur.state = running ? 1 : 0;
            cur.has_rtc = rtc_available ? 1 : 0;
            cur.rtc_start_h = rtc_start_h;
            cur.rtc_start_m = rtc_start_m;
            cur.rtc_start_s = rtc_start_s;
            cur.reserved[0] = 0;
            cur.reserved[1] = 0;
            cur.padding = 0;
            write_sram_record(&cur);
            need_full_redraw = true;
        }

        /* 5. Change Workload Profile: L / R */
        if (key_hit(KEY_L)) {
            mode = (mode - 1 + 3) % 3;
            sound_play_square2(1800, 1, PAN_CENTER);
            if (!running) sound_stop_all();
            need_full_redraw = true;
        }
        if (key_hit(KEY_R)) {
            mode = (mode + 1) % 3;
            sound_play_square2(1900, 1, PAN_CENTER);
            if (!running) sound_stop_all();
            need_full_redraw = true;
        }

        /* 6. Execution & Runtime Accounting */
        bool sec_changed = false;
        if (running) {
            u16 tm3 = REG_TM3CNT_L;
            if (tm3 != last_tm3) {
                u16 delta = (u16)(tm3 - last_tm3);
                current_secs += delta;
                last_tm3 = tm3;
                sec_changed = true;

                /* Refresh RTC once per second if available */
                if (rtc_available) {
                    rtc_read_time(&rtc);
                }

                /* Auto-save to SRAM every 1 second */
                BatteryRecord cur;
                cur.magic = BATTERY_SRAM_MAGIC;
                cur.elapsed_secs = current_secs;
                cur.load_mode = (u8)mode;
                cur.state = 1; /* Actively running */
                cur.has_rtc = rtc_available ? 1 : 0;
                cur.rtc_start_h = rtc_start_h;
                cur.rtc_start_m = rtc_start_m;
                cur.rtc_start_s = rtc_start_s;
                cur.reserved[0] = 0;
                cur.reserved[1] = 0;
                cur.padding = 0;
                write_sram_record(&cur);
            }

            /* Execute selected workload */
            if (mode == 1) {
                if (sec_changed && (current_secs % 10) == 0) {
                    sound_play_square2(1750, 1, PAN_CENTER);
                }
            } else if (mode == 2) {
                /* Audio pulse */
                if ((tick & 31) == 0) {
                    sound_play_square2(1750, 2, PAN_CENTER);
                }
            }
        }

        /* 7. Language Change Check */
        if (g_lang != prev_lang) {
            prev_lang = g_lang;
            need_full_redraw = true;
        }

        /* 8. Full Redraw */
        if (need_full_redraw) {
            gfx_clear(COLOR_NAVY);
            gfx_draw_header(battery_headers[g_lang], "BENCHMARK");
            gfx_draw_footer(battery_footers[g_lang]);

            /* Card 1: Last Saved Test */
            gfx_draw_card(4, 16, 232, 52, card1_titles[g_lang], COLOR_CARD_BORDER);
            if (has_saved && saved_rec.elapsed_secs > 0) {
                u32 s = saved_rec.elapsed_secs;
                u32 hrs = s / 3600;
                u32 min = (s % 3600) / 60;
                u32 sec = s % 60;

                if (g_lang == LANG_DE) {
                    if (saved_rec.state == 1) {
                        gfx_draw_text(8, 28, "STATUS: AKKU VOLLSTAENDIG ENTLADEN!", COLOR_EMERALD, COLOR_CARD_BG, false);
                    } else {
                        gfx_draw_text(8, 28, "STATUS: DURCH BENUTZER GESTOPPT", COLOR_AMBER, COLOR_CARD_BG, false);
                    }
                    gfx_draw_text_fmt(8, 40, COLOR_WHITE, COLOR_CARD_BG, false,
                                      "Dauer : %02u Std. %02u Min. %02u s", hrs, min, sec);
                    if (saved_rec.has_rtc) {
                        gfx_draw_text_fmt(8, 52, COLOR_CYAN, COLOR_CARD_BG, false,
                                          "Start : %02u:%02u:%02u | Modus: %u",
                                          saved_rec.rtc_start_h, saved_rec.rtc_start_m, saved_rec.rtc_start_s,
                                          saved_rec.load_mode + 1);
                    } else {
                        gfx_draw_text_fmt(8, 52, COLOR_CYAN, COLOR_CARD_BG, false,
                                          "Profil: %-22s", mode_names[LANG_DE][saved_rec.load_mode]);
                    }
                } else {
                    if (saved_rec.state == 1) {
                        gfx_draw_text(8, 28, "STATUS: BATTERY FULLY DEPLETED!", COLOR_EMERALD, COLOR_CARD_BG, false);
                    } else {
                        gfx_draw_text(8, 28, "STATUS: STOPPED BY USER", COLOR_AMBER, COLOR_CARD_BG, false);
                    }
                    gfx_draw_text_fmt(8, 40, COLOR_WHITE, COLOR_CARD_BG, false,
                                      "Runtime: %02u h %02u m %02u s", hrs, min, sec);
                    if (saved_rec.has_rtc) {
                        gfx_draw_text_fmt(8, 52, COLOR_CYAN, COLOR_CARD_BG, false,
                                          "Start : %02u:%02u:%02u | Mode: %u",
                                          saved_rec.rtc_start_h, saved_rec.rtc_start_m, saved_rec.rtc_start_s,
                                          saved_rec.load_mode + 1);
                    } else {
                        gfx_draw_text_fmt(8, 52, COLOR_CYAN, COLOR_CARD_BG, false,
                                          "Profile: %-22s", mode_names[LANG_EN][saved_rec.load_mode]);
                    }
                }
            } else {
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 28, "Kein vorheriger Test im SRAM.", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 40, "Starte unten einen Laufzeittest!", COLOR_WHITE, COLOR_CARD_BG, false);
                    gfx_draw_text_fmt(8, 52, sram_ok ? COLOR_EMERALD : COLOR_CORAL, COLOR_CARD_BG, false,
                                      "SRAM: %s", sram_ok ? "BEREIT (AUTO-SAVE)" : "NICHT BESCHREIBBAR");
                } else {
                    gfx_draw_text(8, 28, "No saved test found in SRAM.", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 40, "Start a run-down test below!", COLOR_WHITE, COLOR_CARD_BG, false);
                    gfx_draw_text_fmt(8, 52, sram_ok ? COLOR_EMERALD : COLOR_CORAL, COLOR_CARD_BG, false,
                                      "SRAM: %s", sram_ok ? "READY (AUTO-SAVE)" : "NOT WRITABLE");
                }
            }

            /* Card 2: Active Test */
            gfx_draw_card(4, 72, 232, 74, card2_titles[g_lang], COLOR_CARD_BORDER);

            /* Static Card 2 content */
            if (g_lang == LANG_DE) {
                gfx_draw_text_fmt(8, 84, COLOR_GOLD, COLOR_CARD_BG, false,
                                  "Profil: < %s >", mode_names[LANG_DE][mode]);
            } else {
                gfx_draw_text_fmt(8, 84, COLOR_GOLD, COLOR_CARD_BG, false,
                                  "Mode  : < %s >", mode_names[LANG_EN][mode]);
            }

            /* 3D Viewport outline on right side */
            gfx_draw_rect_outline(182, 94, 44, 40, COLOR_CARD_BORDER);

            cube_rendered = false;
            need_full_redraw = false;
            prev_display_secs = 0xFFFFFFFF; /* Force clock & info redraw */
        }

        /* 9. Live Updates inside Card 2 */
        if (current_secs != prev_display_secs) {
            prev_display_secs = current_secs;
            u32 cur_s = current_secs;
            u32 cur_hrs = cur_s / 3600;
            u32 cur_min = (cur_s % 3600) / 60;
            u32 cur_sec = cur_s % 60;

            /* Line 2: Timebase & RTC */
            gfx_draw_rect(8, 95, 172, 9, COLOR_CARD_BG);
            if (g_lang == LANG_DE) {
                if (rtc_available) {
                    gfx_draw_text_fmt(8, 95, COLOR_CYAN, COLOR_CARD_BG, false,
                                      "Basis: RTC %02u:%02u:%02u (Quarz)",
                                      rtc.hours, rtc.minutes, rtc.seconds);
                } else {
                    gfx_draw_text(8, 95, "Basis: HW-Timer 16.384 kHz", COLOR_CYAN, COLOR_CARD_BG, false);
                }
            } else {
                if (rtc_available) {
                    gfx_draw_text_fmt(8, 95, COLOR_CYAN, COLOR_CARD_BG, false,
                                      "Time : RTC %02u:%02u:%02u (Qrtz)",
                                      rtc.hours, rtc.minutes, rtc.seconds);
                } else {
                    gfx_draw_text(8, 95, "Time : HW-Timer 16.384 kHz", COLOR_CYAN, COLOR_CARD_BG, false);
                }
            }

            /* Line 3: Digital Clock Timer */
            u16 timer_col = running ? COLOR_EMERALD : COLOR_AMBER;
            gfx_draw_rect(8, 107, 170, 10, COLOR_CARD_BG);
            if (g_lang == LANG_DE) {
                gfx_draw_text_fmt(16, 107, timer_col, COLOR_CARD_BG, false,
                                  "[ LAUFZEIT: %02u:%02u:%02u ]", cur_hrs, cur_min, cur_sec);
            } else {
                gfx_draw_text_fmt(16, 107, timer_col, COLOR_CARD_BG, false,
                                  "[ RUNTIME : %02u:%02u:%02u ]", cur_hrs, cur_min, cur_sec);
            }

            /* Line 4: Status label */
            gfx_draw_rect(8, 120, 170, 9, COLOR_CARD_BG);
            if (running) {
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 120, "Status: LAEUFT", COLOR_CYAN, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text(8, 120, "Status: RUNNING", COLOR_CYAN, COLOR_CARD_BG, false);
                }
            } else {
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 120, "Status: PAUSIERT ([A]: Start)", COLOR_AMBER, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text(8, 120, "Status: PAUSED ([A]: Start)", COLOR_AMBER, COLOR_CARD_BG, false);
                }
            }

            /* Line 5: Action info */
            gfx_draw_rect(8, 132, 170, 9, COLOR_CARD_BG);
            if (running) {
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 132, "SRAM Auto-Save (1s) aktiv", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text(8, 132, "SRAM Auto-Save (1s) active", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                }
            } else {
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 132, "START+A: Zaehler reset", COLOR_DARK_GRAY, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text(8, 132, "START+A: Reset timer", COLOR_DARK_GRAY, COLOR_CARD_BG, false);
                }
            }
        }

        /* Heartbeat bar animation (only updates small area at x=106) */
        if (running) {
            int pulse = (tick >> 2) % 6;
            char bar[8];
            for (int p = 0; p < 6; p++) {
                bar[p] = (p == pulse || p == (pulse + 1) % 6) ? '=' : '-';
            }
            bar[6] = '\0';
            gfx_draw_rect(106, 120, 52, 8, COLOR_CARD_BG);
            gfx_draw_text_fmt(106, 120, COLOR_CYAN, COLOR_CARD_BG, false, "[%s]", bar);
        }

        /* 10. 3D Wireframe Cube / Profile Visualizer */
        if (mode == 2) {
            /* Mode 2: Real 3D Wireframe Cube */
            if (running) {
                /* Erase previous wireframe edges */
                if (cube_rendered) {
                    for (int e = 0; e < 12; e++) {
                        u8 i0 = cube_edges[e][0];
                        u8 i1 = cube_edges[e][1];
                        gfx_draw_line(prev_cube_x[i0], prev_cube_y[i0],
                                      prev_cube_x[i1], prev_cube_y[i1], COLOR_CARD_BG);
                    }
                }

                /* Advance rotation angles */
                angle_x = (angle_x + 1) & 63;
                angle_y = (angle_y + 2) & 63;

                s16 sx = f_sin(angle_x), cx = f_cos(angle_x);
                s16 sy = f_sin(angle_y), cy = f_cos(angle_y);

                int cur_x[8], cur_y[8];
                for (int v = 0; v < 8; v++) {
                    s16 vx = cube_v[v][0];
                    s16 vy = cube_v[v][1];
                    s16 vz = cube_v[v][2];

                    s16 x1 = (vx * cy - vz * sy) >> 7;
                    s16 z1 = (vx * sy + vz * cy) >> 7;
                    s16 y1 = (vy * cx - z1 * sx) >> 7;

                    cur_x[v] = 204 + x1;
                    cur_y[v] = 114 + y1;
                }

                /* Draw new edges in emerald green */
                for (int e = 0; e < 12; e++) {
                    u8 i0 = cube_edges[e][0];
                    u8 i1 = cube_edges[e][1];
                    gfx_draw_line(cur_x[i0], cur_y[i0], cur_x[i1], cur_y[i1], COLOR_EMERALD);
                }

                for (int v = 0; v < 8; v++) {
                    prev_cube_x[v] = cur_x[v];
                    prev_cube_y[v] = cur_y[v];
                }
                cube_rendered = true;
            } else if (!cube_rendered) {
                /* Paused: static cube in amber */
                gfx_draw_rect(184, 96, 40, 36, COLOR_CARD_BG);
                s16 sx = f_sin(angle_x), cx = f_cos(angle_x);
                s16 sy = f_sin(angle_y), cy = f_cos(angle_y);
                int cur_x[8], cur_y[8];
                for (int v = 0; v < 8; v++) {
                    s16 vx = cube_v[v][0];
                    s16 vy = cube_v[v][1];
                    s16 vz = cube_v[v][2];
                    s16 x1 = (vx * cy - vz * sy) >> 7;
                    s16 z1 = (vx * sy + vz * cy) >> 7;
                    s16 y1 = (vy * cx - z1 * sx) >> 7;
                    cur_x[v] = 204 + x1;
                    cur_y[v] = 114 + y1;
                }
                for (int e = 0; e < 12; e++) {
                    u8 i0 = cube_edges[e][0];
                    u8 i1 = cube_edges[e][1];
                    gfx_draw_line(cur_x[i0], cur_y[i0], cur_x[i1], cur_y[i1], COLOR_AMBER);
                }
                for (int v = 0; v < 8; v++) {
                    prev_cube_x[v] = cur_x[v];
                    prev_cube_y[v] = cur_y[v];
                }
                cube_rendered = true;
            }
        } else {
            /* Mode 0 or 1: Show text indicator once per redraw */
            if (!cube_rendered) {
                gfx_draw_rect(184, 96, 40, 36, COLOR_CARD_BG);
                if (mode == 0) {
                    gfx_draw_text(186, 110, "[IDLE]", COLOR_DARK_GRAY, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text(186, 110, "[GAME]", COLOR_CYAN, COLOR_CARD_BG, false);
                }
                cube_rendered = true;
            }
        }
    }

    hw_timer_stop();
    sound_stop_all();
}
