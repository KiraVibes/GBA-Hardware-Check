#include "../include/display_test.h"
#include "../include/graphics.h"
#include "../include/input.h"

enum DisplaySubMode {
    DISP_DEAD_PIXEL = 0,
    DISP_COLOR_RAMPS,
    DISP_GHOSTING,
    DISP_GRID_ALIGNMENT,
    DISP_VCOM_FLICKER,
    DISP_IPS_OLED_CALIBRATION,
    DISP_FRAMERATE_SYNC,
    DISP_TEARING_SCROLL,
    DISP_DROP_SHADOW_30HZ,
    DISP_MODE_COUNT
};

static const u16 solid_colors[] = {
    COLOR_WHITE,
    COLOR_BLACK,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_YELLOW,
    COLOR_CYAN,
    COLOR_MAGENTA
};

static const char* solid_color_names[] = {
    "WHITE", "BLACK", "RED", "GREEN", "BLUE", "YELLOW", "CYAN", "MAGENTA"
};

#define SOLID_COUNT 8

/* Fast 32-bit DMA3 helpers (dst/src must be 4-byte aligned) */
static inline void dma3_copy32(void* dst, const void* src, u16 word_count) {
    __asm__ volatile("" ::: "memory");
    REG_DMA3CNT_H = 0;
    REG_DMA3SAD = (u32)src;
    REG_DMA3DAD = (u32)dst;
    REG_DMA3CNT_L = word_count;
    REG_DMA3CNT_H = 0x8400; /* Enable, 32-bit, src inc, dst inc */
}

static inline void dma3_fill32(void* dst, u32 val32, u16 word_count) {
    volatile u32 fill = val32;
    __asm__ volatile("" ::: "memory");
    REG_DMA3CNT_H = 0;
    REG_DMA3SAD = (u32)&fill;
    REG_DMA3DAD = (u32)dst;
    REG_DMA3CNT_L = word_count;
    REG_DMA3CNT_H = 0x8500; /* Enable, 32-bit, src fixed, dst inc */
}

static inline u32 read_cycle_timer(void) {
    u16 h1 = REG_TM1CNT_L;
    u16 l1 = REG_TM0CNT_L;
    u16 h2 = REG_TM1CNT_L;
    if (h1 != h2) l1 = REG_TM0CNT_L;
    return ((u32)h2 << 16) | l1;
}

static inline void stop_cycle_timer(void) {
    REG_TM0CNT_H = 0;
    REG_TM1CNT_H = 0;
}

/* Draw a horizontally screen-wrapped rectangle */
static void draw_wrapped_rect(int x, int y, int w, int h, u16 color) {
    x %= SCREEN_WIDTH;
    if (x < 0) x += SCREEN_WIDTH;
    if (x + w <= SCREEN_WIDTH) {
        gfx_draw_rect(x, y, w, h, color);
    } else {
        int first = SCREEN_WIDTH - x;
        gfx_draw_rect(x, y, first, h, color);
        gfx_draw_rect(0, y, w - first, h, color);
    }
}

/* Restore striped background inside a dirty rect for 30Hz Drop Shadow test */
static void restore_shadow_bg_rect(int rx, int ry, int rw, int rh) {
    if (rw <= 0 || rh <= 0) return;
    gfx_draw_rect(rx, ry, rw, rh, COLOR_CARD_BG);

    int x_end = rx + rw;
    int y_start = (ry < 36) ? 36 : ry;
    int y_end = ((ry + rh) > 120) ? 120 : (ry + rh);
    if (y_start >= y_end) return;

    for (int sx = 20; sx < 220; sx += 30) {
        int ix0 = (rx > sx) ? rx : sx;
        int ix1 = (x_end < (sx + 16)) ? x_end : (sx + 16);
        if (ix0 < ix1) {
            gfx_draw_rect(ix0, y_start, ix1 - ix0, y_end - y_start, COLOR_DARK_GRAY);
        }
    }
}

void run_display_test(void) {
    int mode = DISP_DEAD_PIXEL;
    int prev_mode = -1;
    int solid_idx = 0;
    int prev_solid_idx = -1;
    bool dead_pixel_osd = true;
    bool prev_dead_osd = false;

    int anim_x = 0;
    int anim_speed = 2;
    int prev_speed = -1;
    int prev_anim_x = -1;

    bool grid_osd = true;
    bool prev_grid_osd = false;

    int flicker_pattern = 0;
    int prev_flicker = -1;
    bool vcom_fullscreen = false;
    bool prev_vcom_fs = false;

    int ips_submode = 0;
    int prev_ips_submode = -1;
    bool ips_grid_osd = true;
    bool prev_ips_grid_osd = false;

    /* Frame-Rate Sync variables */
    u32 last_cycles = 0;
    u32 cycles_accum = 0;
    u32 sample_frames = 0;
    u32 total_sync_frames = 0;
    u32 measured_fps_int = 59;
    u32 measured_fps_dec = 728;
    u32 frame_drops = 0;
    u32 prev_frame_drops = 0xFFFFFFFFu;
    u32 last_delta = 280896;
    bool sync_first_frame = true;
    int sync_bar_x = 12;
    int prev_sync_bar_x = -1;

    /* Tearing Scroll variables & IWRAM-aligned scanline LUTs (stack = IWRAM) */
    int tear_pattern = 0;
    int tear_speed = 4;
    int tear_pos = 0;
    bool tear_paused = false;
    bool tear_ui_dirty = true;
    bool tear_frame_dirty = true;
    u16 tear_vert_even[272] __attribute__((aligned(4)));
    u16 tear_vert_odd[272]  __attribute__((aligned(4)));
    u16 tear_diag_even[272] __attribute__((aligned(4)));
    u16 tear_diag_odd[272]  __attribute__((aligned(4)));

    for (int x = 0; x < 272; x++) {
        int bv0 = x & 31;
        int bv1 = (x + 1) & 31;
        tear_vert_even[x] = (bv0 < 16) ? COLOR_WHITE : COLOR_NAVY;
        tear_vert_odd[x]  = (bv1 < 16) ? COLOR_WHITE : COLOR_NAVY;
        tear_diag_even[x] = (bv0 < 8) ? COLOR_CYAN : ((bv0 < 16) ? COLOR_RED : COLOR_BLACK);
        tear_diag_odd[x]  = (bv1 < 8) ? COLOR_CYAN : ((bv1 < 16) ? COLOR_RED : COLOR_BLACK);
    }

    /* 30Hz Drop Shadow variables */
    int shadow_x = 110;
    int prev_shadow_x = -1;
    int shadow_speed = 2;
    int shadow_dir = 1;
    bool shadow_move = true;
    int prev_shadow_speed = -1;
    bool prev_shadow_move = false;
    int frame_tick = 0;

    while (1) {
        vsync();
        input_poll();

        if (key_hit(KEY_B)) {
            stop_cycle_timer();
            break;
        }

        /* Switch modes with L and R (disabled in fullscreen VCOM unless toggled off) */
        if (!vcom_fullscreen) {
            if (key_hit(KEY_R)) {
                mode = (mode + 1) % DISP_MODE_COUNT;
            }
            if (key_hit(KEY_L)) {
                mode = (mode - 1 + DISP_MODE_COUNT) % DISP_MODE_COUNT;
            }
        }

        bool mode_changed = (mode != prev_mode);
        if (mode_changed) {
            stop_cycle_timer();
            gfx_clear(COLOR_BLACK);
        }
        prev_mode = mode;

        switch (mode) {
            case DISP_DEAD_PIXEL: {
                if (key_hit(KEY_A) || key_hit(KEY_RIGHT)) {
                    solid_idx = (solid_idx + 1) % SOLID_COUNT;
                }
                if (key_hit(KEY_LEFT)) {
                    solid_idx = (solid_idx - 1 + SOLID_COUNT) % SOLID_COUNT;
                }
                if (key_hit(KEY_SELECT) || key_hit(KEY_START)) {
                    dead_pixel_osd = !dead_pixel_osd;
                }

                if (mode_changed || solid_idx != prev_solid_idx || dead_pixel_osd != prev_dead_osd) {
                    prev_solid_idx = solid_idx;
                    prev_dead_osd = dead_pixel_osd;

                    gfx_clear(solid_colors[solid_idx]);
                    if (dead_pixel_osd) {
                        u16 bg = solid_colors[solid_idx];
                        u16 text_color = (bg == COLOR_WHITE || bg == COLOR_YELLOW ||
                                          bg == COLOR_CYAN || bg == COLOR_GREEN) ? COLOR_BLACK : COLOR_WHITE;
                        gfx_draw_text_fmt(4, 4, text_color, bg, true,
                                          "DEAD PIXEL [%s] (A/D-Pad)", solid_color_names[solid_idx]);
                        gfx_draw_text(4, SCREEN_HEIGHT - 12,
                                      "SEL:Hide OSD | L/R:Mode | B:Back",
                                      text_color, bg, true);
                    }
                }
                break;
            }

            case DISP_COLOR_RAMPS: {
                if (mode_changed) {
                    gfx_clear(COLOR_BLACK);
                    gfx_draw_header("15-BIT RGB GRADIENT RAMPS", "L/R:MODE");

                    /* Draw 32-step Red Ramp */
                    gfx_draw_text(8, 20, "RED (0..31)", COLOR_RED, COLOR_BLACK, true);
                    for (int i = 0; i < 32; i++) {
                        gfx_draw_rect(8 + i * 7, 30, 7, 18, RGB555(i, 0, 0));
                    }

                    /* Draw 32-step Green Ramp */
                    gfx_draw_text(8, 52, "GREEN (0..31)", COLOR_GREEN, COLOR_BLACK, true);
                    for (int i = 0; i < 32; i++) {
                        gfx_draw_rect(8 + i * 7, 62, 7, 18, RGB555(0, i, 0));
                    }

                    /* Draw 32-step Blue Ramp */
                    gfx_draw_text(8, 84, "BLUE (0..31)", COLOR_CYAN, COLOR_BLACK, true);
                    for (int i = 0; i < 32; i++) {
                        gfx_draw_rect(8 + i * 7, 94, 7, 18, RGB555(0, 0, i));
                    }

                    /* Draw 32-step Grayscale Ramp */
                    gfx_draw_text(8, 116, "GRAYSCALE (0..31)", COLOR_WHITE, COLOR_BLACK, true);
                    for (int i = 0; i < 32; i++) {
                        gfx_draw_rect(8 + i * 7, 126, 7, 18, RGB555(i, i, i));
                    }

                    gfx_draw_footer("IPS/AGS-101 Gamma & Contrast | B:Back");
                }
                break;
            }

            case DISP_GHOSTING: {
                if (mode_changed) {
                    gfx_clear(COLOR_BLACK);
                    gfx_draw_header("GHOSTING & RESPONSE TIME", "UP/DN:SPD");
                    gfx_draw_footer("UP/DN:Speed | L/R:Mode | B:Back");
                    prev_speed = -1;
                    prev_anim_x = -1;
                }

                if (key_hit(KEY_UP) && anim_speed < 8) anim_speed++;
                if (key_hit(KEY_DOWN) && anim_speed > 1) anim_speed--;

                /* Only redraw speed text when value actually changes */
                if (anim_speed != prev_speed) {
                    gfx_draw_rect(6, 18, 160, 11, COLOR_BLACK);
                    gfx_draw_text_fmt(6, 18, COLOR_WHITE, COLOR_BLACK, false, "Speed: %d px/frame", anim_speed);
                    prev_speed = anim_speed;
                }

                if (prev_anim_x < 0) {
                    /* Initial full draw of all 4 bars */
                    for (int b = 0; b < 4; b++) {
                        int nbx = (anim_x + b * 60) % SCREEN_WIDTH;
                        draw_wrapped_rect(nbx, 34, 24, 30, COLOR_WHITE);
                        draw_wrapped_rect(nbx, 70, 24, 30, COLOR_RED);
                        draw_wrapped_rect(nbx, 106, 24, 30, COLOR_CYAN);
                    }
                    prev_anim_x = anim_x;
                } else {
                    /* Incremental edge update: only erase trailing strip & draw leading strip (zero mid-bar flicker) */
                    for (int b = 0; b < 4; b++) {
                        int obx = (prev_anim_x + b * 60) % SCREEN_WIDTH;
                        draw_wrapped_rect(obx, 34, anim_speed, 30, COLOR_BLACK);
                        draw_wrapped_rect(obx, 70, anim_speed, 30, COLOR_BLACK);
                        draw_wrapped_rect(obx, 106, anim_speed, 30, COLOR_BLACK);

                        int lead_x = (obx + 24) % SCREEN_WIDTH;
                        draw_wrapped_rect(lead_x, 34, anim_speed, 30, COLOR_WHITE);
                        draw_wrapped_rect(lead_x, 70, anim_speed, 30, COLOR_RED);
                        draw_wrapped_rect(lead_x, 106, anim_speed, 30, COLOR_CYAN);
                    }
                    anim_x = (prev_anim_x + anim_speed) % SCREEN_WIDTH;
                    prev_anim_x = anim_x;
                }
                break;
            }

            case DISP_GRID_ALIGNMENT: {
                if (key_hit(KEY_SELECT) || key_hit(KEY_START)) {
                    grid_osd = !grid_osd;
                }

                if (mode_changed || grid_osd != prev_grid_osd) {
                    prev_grid_osd = grid_osd;
                    gfx_clear(COLOR_BLACK);

                    /* 16x16 Grid drawn first so borders stay intact */
                    for (int x = 16; x < SCREEN_WIDTH; x += 16) {
                        gfx_draw_line_v(x, 0, SCREEN_HEIGHT, COLOR_DARK_GRAY);
                    }
                    for (int y = 16; y < SCREEN_HEIGHT; y += 16) {
                        gfx_draw_line_h(0, y, SCREEN_WIDTH, COLOR_DARK_GRAY);
                    }

                    /* Center crosshair (240x160 -> center is at 120, 80) */
                    gfx_draw_line_h(0, 80, SCREEN_WIDTH, COLOR_RED);
                    gfx_draw_line_v(120, 0, SCREEN_HEIGHT, COLOR_RED);
                    gfx_draw_circle(120, 80, 20, COLOR_GOLD, false);

                    /* Outer boundary 1-pixel borders drawn last so all 4 edges remain crisp */
                    gfx_draw_rect_outline(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_CYAN);
                    gfx_draw_rect_outline(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, COLOR_AMBER);

                    if (grid_osd) {
                        /* Floating info pill inside borders */
                        gfx_draw_rect(6, 6, 158, 22, COLOR_HEADER_BG);
                        gfx_draw_rect_outline(6, 6, 158, 22, COLOR_CARD_BORDER);
                        gfx_draw_text(10, 9, "CENTER & LENS ALIGNMENT", COLOR_WHITE, COLOR_HEADER_BG, false);
                        gfx_draw_text(10, 18, "Check shell border cutoff", COLOR_LIGHT_GRAY, COLOR_HEADER_BG, false);

                        /* Floating bottom hint pill leaving bottom cyan/amber borders 100% visible */
                        gfx_draw_rect(6, 141, 228, 13, COLOR_HEADER_BG);
                        gfx_draw_rect_outline(6, 141, 228, 13, COLOR_CARD_BORDER);
                        gfx_draw_text(10, 144, "SEL:Hide OSD | L/R:Mode | B:Back", COLOR_GOLD, COLOR_HEADER_BG, false);
                    }
                }
                break;
            }

            case DISP_VCOM_FLICKER: {
                if (key_hit(KEY_A)) {
                    flicker_pattern = !flicker_pattern;
                }
                if (key_hit(KEY_SELECT) || key_hit(KEY_START)) {
                    vcom_fullscreen = !vcom_fullscreen;
                }

                if (mode_changed || flicker_pattern != prev_flicker || vcom_fullscreen != prev_vcom_fs) {
                    prev_flicker = flicker_pattern;
                    prev_vcom_fs = vcom_fullscreen;

                    u16 col_a = COLOR_WHITE;
                    u16 col_b = COLOR_BLACK;

                    int start_y = vcom_fullscreen ? 0 : 15;
                    int end_y = vcom_fullscreen ? SCREEN_HEIGHT : (SCREEN_HEIGHT - 12);

                    if (flicker_pattern == 0) {
                        /* 1x1 checkerboard */
                        for (int y = start_y; y < end_y; y++) {
                            u16* row = &VRAM[y * SCREEN_WIDTH];
                            for (int x = 0; x < SCREEN_WIDTH; x++) {
                                row[x] = ((x ^ y) & 1) ? col_a : col_b;
                            }
                        }
                    } else {
                        /* 2x2 checkerboard */
                        for (int y = start_y; y < end_y; y++) {
                            u16* row = &VRAM[y * SCREEN_WIDTH];
                            for (int x = 0; x < SCREEN_WIDTH; x++) {
                                row[x] = (((x >> 1) ^ (y >> 1)) & 1) ? col_a : col_b;
                            }
                        }
                    }

                    if (!vcom_fullscreen) {
                        gfx_draw_header("VCOM / BIAS FLICKER TEST", flicker_pattern ? "2x2" : "1x1");
                        gfx_draw_footer("A:1x1/2x2 | SEL:Fullscr | B:Back");
                    }
                }
                break;
            }

            case DISP_IPS_OLED_CALIBRATION: {
                if (key_hit(KEY_A)) {
                    ips_submode = !ips_submode;
                }
                if (ips_submode == 1 && (key_hit(KEY_SELECT) || key_hit(KEY_START))) {
                    ips_grid_osd = !ips_grid_osd;
                }

                if (mode_changed || ips_submode != prev_ips_submode ||
                    (ips_submode == 1 && ips_grid_osd != prev_ips_grid_osd)) {
                    prev_ips_submode = ips_submode;
                    prev_ips_grid_osd = ips_grid_osd;

                    if (ips_submode == 0) {
                        gfx_clear(COLOR_NAVY);
                        gfx_draw_header("IPS & OLED CALIBRATION", "GAMMA");
                        gfx_draw_footer("A:1px Grid | L/R:Mode | B:Back");

                        /* Card 1: Shadow Detail (Near Black RGB 0..5) */
                        gfx_draw_card(4, 16, 232, 60, "NEAR-BLACK SHADOW DETAIL (RGB 0..5)", COLOR_CARD_BORDER);
                        for (int i = 0; i <= 5; i++) {
                            int bx = 14 + i * 36;
                            u16 col = RGB555(i, i, i);
                            gfx_draw_rect(bx, 29, 32, 20, col);
                            gfx_draw_rect_outline(bx, 29, 32, 20, COLOR_DARK_GRAY);
                            gfx_draw_text_fmt(bx + 13, 51, (i == 0) ? COLOR_LIGHT_GRAY : COLOR_WHITE,
                                              COLOR_CARD_BG, false, "%d", i);
                        }
                        gfx_draw_text(8, 63, "If 1-2 match 0: Blacks crushed!", COLOR_AMBER, COLOR_CARD_BG, false);

                        /* Card 2: Highlight Clipping (RGB 26..31) */
                        gfx_draw_card(4, 80, 232, 62, "HIGHLIGHT CLIPPING (RGB 26..31)", COLOR_CARD_BORDER);
                        for (int i = 0; i <= 5; i++) {
                            int level = 26 + i;
                            int bx = 14 + i * 36;
                            u16 col = RGB555(level, level, level);
                            gfx_draw_rect(bx, 93, 32, 20, col);
                            gfx_draw_rect_outline(bx, 93, 32, 20, COLOR_DARK_GRAY);
                            gfx_draw_text_fmt(bx + 10, 115, (level == 31) ? COLOR_GOLD : COLOR_LIGHT_GRAY,
                                              COLOR_CARD_BG, false, "%d", level);
                        }
                        gfx_draw_text(8, 128, "If 30 blends into 31: Clipped!", COLOR_CORAL, COLOR_CARD_BG, false);
                    } else {
                        /* Submode 1: 1-Pixel Scanline & Subpixel Integer Scaling Grid */
                        gfx_clear(COLOR_BLACK);
                        int start_y = ips_grid_osd ? 15 : 0;
                        int end_y = ips_grid_osd ? (SCREEN_HEIGHT - 12) : SCREEN_HEIGHT;

                        /* Left half: alternating horizontal 1px scanlines */
                        for (int y = start_y; y < end_y; y++) {
                            if ((y & 1) == 0) {
                                gfx_draw_line_h(0, y, 120, COLOR_WHITE);
                            }
                        }
                        /* Right half: alternating vertical 1px stripes */
                        for (int x = 120; x < SCREEN_WIDTH; x += 2) {
                            gfx_draw_line_v(x, start_y, end_y - start_y, COLOR_WHITE);
                        }
                        gfx_draw_line_v(120, start_y, end_y - start_y, COLOR_RED);

                        if (ips_grid_osd) {
                            gfx_draw_rect(0, 0, SCREEN_WIDTH, 14, COLOR_HEADER_BG);
                            gfx_draw_line_h(0, 14, SCREEN_WIDTH, COLOR_CYAN);
                            gfx_draw_text(14, 3, "1PX HORIZ LINES", COLOR_CYAN, COLOR_HEADER_BG, false);
                            gfx_draw_text(132, 3, "1PX VERT STRIPES", COLOR_CYAN, COLOR_HEADER_BG, false);
                            gfx_draw_footer("A:Swatches | SEL:OSD | B:Back");
                        }
                    }
                }
                break;
            }

            case DISP_FRAMERATE_SYNC: {
                if (mode_changed) {
                    gfx_clear(COLOR_NAVY);
                    gfx_draw_header("FRAME-RATE & SYNC TEST", "59.73 HZ");
                    gfx_draw_footer("A:Reset Drops | L/R:Mode | B:Back");

                    /* Static Card 1: Live Measured Timing */
                    gfx_draw_card(4, 16, 232, 52, "LIVE HARDWARE REFRESH RATE", COLOR_CARD_BORDER);
                    gfx_draw_text(8, 29, "Measured FPS: 59.728 Hz", COLOR_WHITE, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 41, "Nominal GBA:  59.728 Hz (280896 cyc)", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 53, "Last Frame:   280896 cyc/frame", COLOR_CYAN, COLOR_CARD_BG, false);

                    /* Static Card 2: Sync & Visual IPS Scaler Stutter Track */
                    gfx_draw_card(4, 72, 232, 72, "SYNC & IPS SCALER STUTTER TEST", COLOR_CARD_BORDER);

                    /* Smooth 60 FPS visual motion track (exposes 60Hz IPS 3.6s micro-stutter) */
                    gfx_draw_rect(10, 123, 220, 14, COLOR_BLACK);
                    gfx_draw_rect_outline(10, 123, 220, 14, COLOR_DARK_GRAY);

                    /* Init Timer 0 (prescaler 1) + Timer 1 (cascade) for 32-bit cycle counter */
                    stop_cycle_timer();
                    REG_TM0CNT_L = 0;
                    REG_TM1CNT_L = 0;
                    REG_TM0CNT_H = 0x0080; /* Start TM0 */
                    REG_TM1CNT_H = 0x0084; /* Start TM1 in cascade mode */

                    last_cycles = read_cycle_timer();
                    sample_frames = 0;
                    total_sync_frames = 0;
                    cycles_accum = 0;
                    frame_drops = 0;
                    prev_frame_drops = 0xFFFFFFFFu;
                    measured_fps_int = 59;
                    measured_fps_dec = 728;
                    sync_first_frame = true;
                    sync_bar_x = 12;
                    prev_sync_bar_x = -1;
                }

                if (key_hit(KEY_A)) {
                    frame_drops = 0;
                    total_sync_frames = 0;
                    sample_frames = 0;
                    cycles_accum = 0;
                    prev_frame_drops = 0xFFFFFFFFu;
                    sync_first_frame = true;
                    last_cycles = read_cycle_timer();
                }

                if (sync_first_frame) {
                    /* First frame initializes reference timestamp; skip partial-frame delta */
                    last_cycles = read_cycle_timer();
                    sync_first_frame = false;
                } else {
                    u32 cur_cycles = read_cycle_timer();
                    u32 delta = cur_cycles - last_cycles;
                    last_cycles = cur_cycles;
                    last_delta = delta;
                    total_sync_frames++;

                    /* Check for VBlank timing drops (> 1.25x or < 0.75x nominal 280,896 cycles) */
                    if (total_sync_frames > 2 && (delta > 350000 || delta < 200000)) {
                        frame_drops++;
                    }

                    cycles_accum += delta;
                    sample_frames++;

                    if (sample_frames >= 60) {
                        u64 total_ticks = (u64)16777216 * 60 * 1000ULL;
                        if (cycles_accum > 0) {
                            u64 mhz = (total_ticks + (cycles_accum >> 1)) / cycles_accum;
                            measured_fps_int = (u32)(mhz / 1000);
                            measured_fps_dec = (u32)(mhz % 1000);
                        }
                        gfx_draw_text_fmt(92, 29, COLOR_WHITE, COLOR_CARD_BG, false,
                                          "%2u.%03u Hz", measured_fps_int, measured_fps_dec);
                        sample_frames = 0;
                        cycles_accum = 0;
                    }

                    /* Lightweight periodic update of last-frame cycle count (zero flicker) */
                    if ((total_sync_frames & 3) == 0) {
                        gfx_draw_text_fmt(92, 53, COLOR_CYAN, COLOR_CARD_BG, false,
                                          "%6u cyc/frame", last_delta);
                    }
                }

                /* Update Card 2 drop status ONLY when value changes */
                if (frame_drops != prev_frame_drops) {
                    prev_frame_drops = frame_drops;
                    gfx_draw_rect(8, 85, 224, 34, COLOR_CARD_BG);
                    if (frame_drops == 0) {
                        gfx_draw_text(8, 85, "Frame Drops: 0 [59.73 HZ LOCK]", COLOR_EMERALD, COLOR_CARD_BG, false);
                        gfx_draw_text(8, 97, "Hardware VBlank timing stable.", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                        gfx_draw_text(8, 109, "Watch bar below for IPS stutter:", COLOR_WHITE, COLOR_CARD_BG, false);
                    } else {
                        gfx_draw_text_fmt(8, 85, COLOR_CORAL, COLOR_CARD_BG, false,
                                          "Frame Drops: %u [TIMING DROP]", frame_drops);
                        gfx_draw_text(8, 97, "Irregular VBlank interval detected!", COLOR_AMBER, COLOR_CARD_BG, false);
                        gfx_draw_text(8, 109, "Check clock/emulator or press A.", COLOR_WHITE, COLOR_CARD_BG, false);
                    }
                }

                /* Update smooth 60 FPS sync bar (only ~40 pixels changed per frame) */
                if (prev_sync_bar_x >= 0) {
                    gfx_draw_rect(prev_sync_bar_x, 125, 8, 10, COLOR_BLACK);
                }
                gfx_draw_rect(sync_bar_x, 125, 8, 10, COLOR_WHITE);
                prev_sync_bar_x = sync_bar_x;
                sync_bar_x += 2;
                if (sync_bar_x > 220) sync_bar_x = 12;
                break;
            }

            case DISP_TEARING_SCROLL: {
                if (mode_changed) {
                    tear_pos = 0;
                    tear_speed = 4;
                    tear_pattern = 0;
                    tear_paused = false;
                    tear_ui_dirty = true;
                    tear_frame_dirty = true;
                }

                if (key_hit(KEY_RIGHT) && tear_speed < 16) {
                    tear_speed++;
                    tear_ui_dirty = true;
                }
                if (key_hit(KEY_LEFT) && tear_speed > 1) {
                    tear_speed--;
                    tear_ui_dirty = true;
                }
                if (key_hit(KEY_A)) {
                    tear_pattern = (tear_pattern + 1) % 3;
                    tear_ui_dirty = true;
                    tear_frame_dirty = true;
                }
                if (key_hit(KEY_START)) {
                    tear_paused = !tear_paused;
                    tear_ui_dirty = true;
                }

                if (!tear_paused) {
                    /* Seamless 32-pixel period wrap (zero jump / inversion) */
                    tear_pos = (tear_pos + tear_speed) & 31;
                    tear_frame_dirty = true;
                }

                /* DMA3 32-bit pattern blitting inside VBlank (y = 15..147 only) */
                if (tear_frame_dirty) {
                    tear_frame_dirty = false;
                    if (tear_pattern == 0) {
                        /* High-Speed Vertical Raster Stripes */
                        const u16* src = (tear_pos & 1) ? &tear_vert_odd[tear_pos - 1]
                                                        : &tear_vert_even[tear_pos];
                        for (int y = 15; y < SCREEN_HEIGHT - 12; y++) {
                            dma3_copy32(&VRAM[y * SCREEN_WIDTH], src, SCREEN_WIDTH / 2);
                        }
                    } else if (tear_pattern == 1) {
                        /* Horizontal scrolling raster bars */
                        u32 col_a = (u32)COLOR_AMBER | ((u32)COLOR_AMBER << 16);
                        u32 col_b = (u32)COLOR_CARD_BG | ((u32)COLOR_CARD_BG << 16);
                        for (int y = 15; y < SCREEN_HEIGHT - 12; y++) {
                            int band = (y + tear_pos) & 31;
                            dma3_fill32(&VRAM[y * SCREEN_WIDTH], (band < 16) ? col_a : col_b, SCREEN_WIDTH / 2);
                        }
                    } else {
                        /* Diagonal cross-hatch tearing pattern */
                        for (int y = 15; y < SCREEN_HEIGHT - 12; y++) {
                            int off = (y + tear_pos) & 31;
                            const u16* src = (off & 1) ? &tear_diag_odd[off - 1]
                                                       : &tear_diag_even[off];
                            dma3_copy32(&VRAM[y * SCREEN_WIDTH], src, SCREEN_WIDTH / 2);
                        }
                    }
                }

                /* Redraw OSD Header & Footer ONLY when state changes (zero flicker) */
                if (tear_ui_dirty) {
                    tear_ui_dirty = false;
                    gfx_draw_rect(0, 0, SCREEN_WIDTH, 14, COLOR_HEADER_BG);
                    gfx_draw_line_h(0, 14, SCREEN_WIDTH, COLOR_CYAN);
                    gfx_draw_text_fmt(4, 3, COLOR_CYAN, COLOR_HEADER_BG, false,
                                      "TEARING [%s] %2dpx/f%s",
                                      (tear_pattern == 0) ? "VERT" : (tear_pattern == 1 ? "HORIZ" : "DIAG"),
                                      tear_speed,
                                      tear_paused ? " PAU" : "");
                    gfx_draw_text(188, 3, "L/R:MODE", COLOR_AMBER, COLOR_HEADER_BG, false);

                    gfx_draw_footer("D-Pad:Spd | A:Pat | START:Pau | B:Back");
                }
                break;
            }

            case DISP_DROP_SHADOW_30HZ: {
                frame_tick++;
                if (mode_changed) {
                    gfx_clear(COLOR_NAVY);
                    gfx_draw_header("30 HZ DROP SHADOW TEST", "30 HZ");
                    gfx_draw_footer("A:Move/Pau | D-Pad:Spd | B:Back");

                    /* Static panel drawn ONCE */
                    gfx_draw_rect(6, 18, 228, 126, COLOR_CARD_BG);
                    gfx_draw_rect_outline(6, 18, 228, 126, COLOR_CARD_BORDER);

                    /* Static vertical background stripes (y=36..120) */
                    restore_shadow_bg_rect(10, 36, 220, 84);

                    /* Static labels & left box outline */
                    gfx_draw_text(14, 22, "STATIC 30HZ", COLOR_WHITE, COLOR_CARD_BG, false);
                    gfx_draw_rect_outline(24, 52, 44, 44, COLOR_CYAN);
                    gfx_draw_text(12, 128, "Good: 50% opacity | Bad: Smear/Trail", COLOR_AMBER, COLOR_CARD_BG, false);

                    shadow_x = 110;
                    prev_shadow_x = -1;
                    shadow_speed = 2;
                    shadow_dir = 1;
                    shadow_move = true;
                    prev_shadow_speed = -1;
                    prev_shadow_move = false;
                }

                if (key_hit(KEY_A)) shadow_move = !shadow_move;
                if (key_hit(KEY_RIGHT) && shadow_speed < 6) shadow_speed++;
                if (key_hit(KEY_LEFT) && shadow_speed > 1) shadow_speed--;

                if (shadow_speed != prev_shadow_speed || shadow_move != prev_shadow_move) {
                    prev_shadow_speed = shadow_speed;
                    prev_shadow_move = shadow_move;
                    gfx_draw_rect(114, 22, 114, 9, COLOR_CARD_BG);
                    if (shadow_move) {
                        gfx_draw_text_fmt(114, 22, COLOR_WHITE, COLOR_CARD_BG, false,
                                          "MOVING (SPD:%d)", shadow_speed);
                    } else {
                        gfx_draw_text(114, 22, "MOVING (PAUSE)", COLOR_GOLD, COLOR_CARD_BG, false);
                    }
                }

                /* 1. Stationary 30Hz shadow box interior (x=25..66, y=53..94) */
                if (frame_tick & 1) {
                    gfx_draw_rect(25, 53, 42, 42, COLOR_BLACK);
                } else {
                    restore_shadow_bg_rect(25, 53, 42, 42);
                }

                /* 2. Moving 30Hz shadow box on right (x=110..180, y=56..91) */
                if (prev_shadow_x >= 0) {
                    restore_shadow_bg_rect(prev_shadow_x, 56, 36, 36);
                }
                if (shadow_move) {
                    shadow_x += shadow_dir * shadow_speed;
                    if (shadow_x >= 180) {
                        shadow_x = 180;
                        shadow_dir = -1;
                    } else if (shadow_x <= 110) {
                        shadow_x = 110;
                        shadow_dir = 1;
                    }
                }
                if (frame_tick & 1) {
                    gfx_draw_rect(shadow_x + 1, 57, 34, 34, COLOR_BLACK);
                }
                gfx_draw_rect_outline(shadow_x, 56, 36, 36, COLOR_GOLD);
                prev_shadow_x = shadow_x;
                break;
            }

            default:
                break;
        }
    }
}
