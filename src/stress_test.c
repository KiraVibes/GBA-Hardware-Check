#include "../include/stress_test.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/sound.h"
#include "../include/help_guide.h"

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

/* 3D Cube vertices: scaled to +/-14 so rotation never exceeds card */
static const s16 cube_v[8][3] = {
    {-14, -14, -14}, { 14, -14, -14}, { 14,  14, -14}, {-14,  14, -14},
    {-14, -14,  14}, { 14, -14,  14}, { 14,  14,  14}, {-14,  14,  14}
};

/* 12 Edges */
static const u8 cube_edges[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0},
    {4,5}, {5,6}, {6,7}, {7,4},
    {0,4}, {1,5}, {2,6}, {3,7}
};

/* 8KB stress buffer in EWRAM */
static vu16 stress_buf[4096] __attribute__((section(".bss")));

static void stress_timer_start(void) {
    REG_TM0CNT_H = 0;
    REG_TM1CNT_H = 0;
    REG_TM0CNT_L = 0;
    REG_TM1CNT_L = 0;
    REG_TM0CNT_H = 0x0080; /* 16.78 MHz */
    REG_TM1CNT_H = 0x0084; /* Cascade count-up */
}

static inline void stress_timer_stop(void) {
    REG_TM0CNT_H = 0;
    REG_TM1CNT_H = 0;
}

static inline u32 stress_timer_read(void) {
    u16 h1 = REG_TM1CNT_L;
    u16 l1 = REG_TM0CNT_L;
    u16 h2 = REG_TM1CNT_L;
    if (h1 != h2) l1 = REG_TM0CNT_L;
    return ((u32)h2 << 16) | l1;
}

static void draw_stress_static_ui(void) {
    gfx_clear(COLOR_NAVY);
    gfx_draw_header("POWER-RAIL & CAP STRESS", "MAX LOAD");
    gfx_draw_footer("A:Audio | START:Rst | SEL:Help | B");

    /* Card 1: Power Rail Monitor & Guidance */
    gfx_draw_card(4, 16, 126, 88, "RAIL STABILITY", COLOR_CARD_BORDER);

    /* Card 2: 3D Wireframe Render Area */
    gfx_draw_card(134, 16, 102, 88, "100% CPU 3D LOAD", COLOR_CARD_BORDER);

    /* Card 3: Physical Capacitor Diagnosis Guide */
    gfx_draw_card(4, 107, 232, 39, "POWER SWITCH & ELKO RECAP GUIDE", COLOR_CARD_BORDER);
    gfx_draw_text(8, 119, "LED flickers RED under max load?", COLOR_AMBER, COLOR_CARD_BG, false);
    gfx_draw_text(8, 130, "Clean power switch / recap C35-37.", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
}

void run_stress_test(void) {
    sound_stop_all();
    sound_init();

    bool audio_load = true;
    int angle_x = 0;
    int angle_y = 0;

    u32 frame_cycles = 280896;
    u32 min_cycles = 280896;
    u32 max_cycles = 280896;
    u32 drop_frames = 0;
    u32 total_frames = 0;
    bool first_frame = true;
    u32 last_cycles = 0;

    int prev_x2d[8];
    int prev_y2d[8];
    bool has_prev_cube = false;
    bool ui_stats_dirty = true;

    draw_stress_static_ui();
    stress_timer_start();

    while (1) {
        /* Single VSync per loop = true 60 FPS stress load & accurate frame period */
        vsync();
        u32 cur_cycles = stress_timer_read();
        if (first_frame) {
            last_cycles = cur_cycles;
            first_frame = false;
        } else {
            frame_cycles = cur_cycles - last_cycles;
            last_cycles = cur_cycles;
            total_frames++;
            if (total_frames > 2) {
                if (frame_cycles > max_cycles) max_cycles = frame_cycles;
                if (frame_cycles < min_cycles) min_cycles = frame_cycles;
                if (frame_cycles > 286000 || frame_cycles < 276000) drop_frames++;
            }
        }

        input_poll();

        if (key_hit(KEY_B)) {
            stress_timer_stop();
            sound_stop_all();
            break;
        }

        if (key_hit(KEY_SELECT)) {
            stress_timer_stop();
            sound_stop_all();
            run_help_guide(HELP_TOPIC_STRESS);
            sound_init();
            draw_stress_static_ui();
            has_prev_cube = false;
            ui_stats_dirty = true;
            first_frame = true;
            stress_timer_start();
            continue;
        }

        if (key_hit(KEY_A)) {
            audio_load = !audio_load;
            if (!audio_load) {
                sound_stop_all();
            }
            ui_stats_dirty = true;
        }

        if (key_hit(KEY_START)) {
            min_cycles = 280896;
            max_cycles = 280896;
            drop_frames = 0;
            total_frames = 0;
            first_frame = true;
            ui_stats_dirty = true;
        }

        /* 1. Heavy Memory Bus Burst */
        u16 pattern = (u16)(total_frames ^ 0x55AA);
        for (int i = 0; i < 4096; i += 4) {
            stress_buf[i] = pattern;
            stress_buf[i + 1] = pattern;
            stress_buf[i + 2] = pattern;
            stress_buf[i + 3] = pattern;
        }

        /* 2. Heavy 4-Channel Audio Current Draw */
        if (audio_load && (total_frames & 7) == 0) {
            sound_play_square1(1200, 2, 0, false, PAN_CENTER);
            sound_play_square2(1800, 1, PAN_CENTER);
            sound_play_wave(1600, 0, PAN_CENTER);
            sound_play_noise(4, false, PAN_CENTER);
        }

        /* 3. 3D Wireframe Cube Calculations & Flicker-Free Edge Erase/Draw */
        angle_x = (angle_x + 1) & 63;
        angle_y = (angle_y + 2) & 63;
        s16 sx = f_sin(angle_x), cx = f_cos(angle_x);
        s16 sy = f_sin(angle_y), cy = f_cos(angle_y);

        int cur_x2d[8];
        int cur_y2d[8];

        for (int i = 0; i < 8; i++) {
            s16 x0 = cube_v[i][0];
            s16 y0 = cube_v[i][1];
            s16 z0 = cube_v[i][2];

            /* Rotate around X */
            s16 y1 = (y0 * cx - z0 * sx) >> 7;
            s16 z1 = (y0 * sx + z0 * cx) >> 7;

            /* Rotate around Y */
            s16 x2 = (x0 * cy + z1 * sy) >> 7;
            s16 z2 = (-x0 * sy + z1 * cy) >> 7;

            /* 3D Perspective projection centered at (184, 64) */
            int dist = z2 + 80;
            if (dist < 20) dist = 20;
            cur_x2d[i] = 184 + (x2 * 48) / dist;
            cur_y2d[i] = 64 + (y1 * 48) / dist;
        }

        /* Erase ONLY previous edges (zero Card 2 flicker) */
        if (has_prev_cube) {
            for (int e = 0; e < 12; e++) {
                int a = cube_edges[e][0];
                int b = cube_edges[e][1];
                gfx_draw_line_clipped(prev_x2d[a], prev_y2d[a], prev_x2d[b], prev_y2d[b],
                                      136, 28, 233, 101, COLOR_CARD_BG);
            }
        }

        /* Draw 12 cube edges strictly clipped to Card 2 interior [136, 28, 233, 101] */
        u16 wire_col = audio_load ? COLOR_CORAL : COLOR_CYAN;
        for (int e = 0; e < 12; e++) {
            int a = cube_edges[e][0];
            int b = cube_edges[e][1];
            gfx_draw_line_clipped(cur_x2d[a], cur_y2d[a], cur_x2d[b], cur_y2d[b],
                                  136, 28, 233, 101, wire_col);
        }

        for (int i = 0; i < 8; i++) {
            prev_x2d[i] = cur_x2d[i];
            prev_y2d[i] = cur_y2d[i];
        }
        has_prev_cube = true;

        /* 4. Live Stability Diagnostics Readout inside Card 1 (throttled to avoid flicker) */
        if (ui_stats_dirty || (total_frames & 7) == 0) {
            ui_stats_dirty = false;
            bool frame_ok = (frame_cycles >= 276000 && frame_cycles <= 286000);
            u16 stat_col = frame_ok ? COLOR_EMERALD : COLOR_RED;

            gfx_draw_text_fmt(8, 30, stat_col, COLOR_CARD_BG, false, "Frm:%6u cyc  ", frame_cycles);
            gfx_draw_text_fmt(8, 42, COLOR_WHITE, COLOR_CARD_BG, false, "Min:%6u cyc  ", min_cycles);
            gfx_draw_text_fmt(8, 54, COLOR_WHITE, COLOR_CARD_BG, false, "Max:%6u cyc  ", max_cycles);
            gfx_draw_text_fmt(8, 66, (drop_frames > 0) ? COLOR_RED : COLOR_EMERALD, COLOR_CARD_BG, false,
                              "Sync: %-11s", (drop_frames == 0) ? "STABLE [OK]" : "UNSTABLE!  ");
            gfx_draw_text_fmt(8, 78, audio_load ? COLOR_AMBER : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false,
                              "Audio: %-10s", audio_load ? "MAX (4 CH)" : "OFF       ");
            gfx_draw_text_fmt(8, 90, (drop_frames > 0) ? COLOR_RED : COLOR_LIGHT_GRAY, COLOR_CARD_BG, false,
                              "Drops: %-10u", drop_frames);
        }
    }
}
