#include "../include/input.h"
#include "../include/graphics.h"

InputState g_input;

static const u16 key_masks[10] = {
    KEY_A, KEY_B, KEY_SELECT, KEY_START,
    KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN,
    KEY_R, KEY_L
};

void input_init(void) {
    g_input.current = 0;
    g_input.previous = 0;
    g_input.pressed = 0;
    g_input.released = 0;
    for (int i = 0; i < 10; i++) {
        g_input.press_counts[i] = 0;
        g_input.hold_frames[i] = 0;
    }
}

void input_poll(void) {
    g_input.previous = g_input.current;
    /* In REG_KEYINPUT: 0 = pressed, 1 = released */
    g_input.current = (u16)((~REG_KEYINPUT) & KEY_ANY);
    g_input.pressed = (u16)(g_input.current & (u16)(~g_input.previous));
    g_input.released = (u16)((u16)(~g_input.current) & g_input.previous);

    for (int i = 0; i < 10; i++) {
        if (g_input.pressed & key_masks[i]) {
            g_input.press_counts[i]++;
        }
        if (g_input.current & key_masks[i]) {
            g_input.hold_frames[i]++;
        } else {
            g_input.hold_frames[i] = 0;
        }
    }

#ifdef STANDALONE_MULTIBOOT
    /* In MultiBoot slave session: keep send register primed with current key state */
    REG_SIOMLT_SEND = (u16)(0x2000 | (g_input.current & KEY_ANY));
#endif
}

/* Specific button drawing functions (only called when state changes!) */
static void draw_btn_l(bool pressed) {
    u16 col = pressed ? COLOR_AMBER : COLOR_DARK_GRAY;
    gfx_draw_rect(14, 32, 28, 8, col);
    gfx_draw_text(22, 32, "L", COLOR_WHITE, col, true);
}

static void draw_btn_r(bool pressed) {
    u16 col = pressed ? COLOR_AMBER : COLOR_DARK_GRAY;
    gfx_draw_rect(104, 32, 28, 8, col);
    gfx_draw_text(115, 32, "R", COLOR_WHITE, col, true);
}

static void draw_btn_up(bool pressed) {
    u16 col = pressed ? COLOR_CYAN : COLOR_DARK_GRAY;
    gfx_draw_rect(34, 60, 12, 12, col);
}

static void draw_btn_down(bool pressed) {
    u16 col = pressed ? COLOR_CYAN : COLOR_DARK_GRAY;
    gfx_draw_rect(34, 84, 12, 12, col);
}

static void draw_btn_left(bool pressed) {
    u16 col = pressed ? COLOR_CYAN : COLOR_DARK_GRAY;
    gfx_draw_rect(22, 72, 12, 12, col);
}

static void draw_btn_right(bool pressed) {
    u16 col = pressed ? COLOR_CYAN : COLOR_DARK_GRAY;
    gfx_draw_rect(46, 72, 12, 12, col);
}

static void draw_btn_a(bool pressed) {
    u16 col = pressed ? COLOR_EMERALD : COLOR_DARK_GRAY;
    gfx_draw_circle(118, 60, 8, col, true);
    gfx_draw_text(115, 56, "A", COLOR_WHITE, col, true);
}

static void draw_btn_b(bool pressed) {
    u16 col = pressed ? COLOR_CORAL : COLOR_DARK_GRAY;
    gfx_draw_circle(100, 72, 8, col, true);
    gfx_draw_text(97, 68, "B", COLOR_WHITE, col, true);
}

static void draw_btn_sel(bool pressed) {
    u16 col = pressed ? COLOR_GOLD : COLOR_DARK_GRAY;
    gfx_draw_rect(58, 100, 14, 6, col);
}

static void draw_btn_str(bool pressed) {
    u16 col = pressed ? COLOR_GOLD : COLOR_DARK_GRAY;
    gfx_draw_rect(76, 100, 14, 6, col);
}

void run_input_test(void) {
    /* Reset counters & hold frames upon entering test */
    input_init();

    /* 1. Initial complete screen draw (done once, zero flicker) */
    gfx_clear(COLOR_NAVY);
    gfx_draw_header("GAMEPAD & BUTTON TEST", "HOLD B: EXIT");
    gfx_draw_footer("Hold B: Exit | Test all buttons live");

    /* Containers */
    gfx_draw_card(6, 18, 140, 126, "CONTROLLER STATE", COLOR_CARD_BORDER);
    gfx_draw_card(150, 18, 84, 126, "DIAGNOSTICS", COLOR_CARD_BORDER);

    /* Static Controller elements */
    draw_btn_l(false);
    draw_btn_r(false);
    draw_btn_up(false);
    draw_btn_down(false);
    draw_btn_left(false);
    draw_btn_right(false);
    gfx_draw_rect(34, 72, 12, 12, COLOR_DARK_GRAY); /* D-Pad center */
    draw_btn_a(false);
    draw_btn_b(false);
    draw_btn_sel(false);
    draw_btn_str(false);
    gfx_draw_text(54, 108, "SEL", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
    gfx_draw_text(74, 108, "STR", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);

    /* Static Diagnostics labels */
    gfx_draw_text(154, 31, "Pivot: OK", COLOR_EMERALD, COLOR_CARD_BG, false);
    gfx_draw_text(154, 43, "Diagonals:", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
    gfx_draw_text(154, 52, "UL:", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
    gfx_draw_text(176, 52, "WAIT", COLOR_DARK_GRAY, COLOR_CARD_BG, false);
    gfx_draw_text(154, 61, "UR:", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
    gfx_draw_text(176, 61, "WAIT", COLOR_DARK_GRAY, COLOR_CARD_BG, false);
    gfx_draw_text(154, 70, "DL:", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
    gfx_draw_text(176, 70, "WAIT", COLOR_DARK_GRAY, COLOR_CARD_BG, false);
    gfx_draw_text(154, 79, "DR:", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
    gfx_draw_text(176, 79, "WAIT", COLOR_DARK_GRAY, COLOR_CARD_BG, false);

    gfx_draw_text(154, 91, "Counts:", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
    gfx_draw_text_fmt(154, 100, COLOR_WHITE, COLOR_CARD_BG, false, "A:00 B:00");
    gfx_draw_text_fmt(154, 110, COLOR_WHITE, COLOR_CARD_BG, false, "L:00 R:00");
    gfx_draw_text(154, 122, "Combo: IDLE", COLOR_DARK_GRAY, COLOR_CARD_BG, false);

    bool diag_ur = false, diag_ul = false, diag_dr = false, diag_dl = false;
    bool prev_opp_err = false;
    bool prev_soft_reset = false;

    while (1) {
        vsync();
        input_poll();

        /* Exit condition: only holding B for 45 frames (SELECT+START removed) */
        if (g_input.hold_frames[BTN_IDX_B] > 45) {
            break;
        }

        /* 2. Dirty-State Redraw: ONLY redraw buttons that changed state! */
        u16 changed = g_input.pressed | g_input.released;

        if (changed & KEY_L) draw_btn_l(key_down(KEY_L));
        if (changed & KEY_R) draw_btn_r(key_down(KEY_R));
        if (changed & KEY_UP) draw_btn_up(key_down(KEY_UP));
        if (changed & KEY_DOWN) draw_btn_down(key_down(KEY_DOWN));
        if (changed & KEY_LEFT) draw_btn_left(key_down(KEY_LEFT));
        if (changed & KEY_RIGHT) draw_btn_right(key_down(KEY_RIGHT));
        if (changed & KEY_A) draw_btn_a(key_down(KEY_A));
        if (changed & KEY_B) draw_btn_b(key_down(KEY_B));
        if (changed & KEY_SELECT) draw_btn_sel(key_down(KEY_SELECT));
        if (changed & KEY_START) draw_btn_str(key_down(KEY_START));

        /* Diagonals check: require BOTH directions pressed simultaneously */
        if (!diag_ur && keys_all_down(KEY_UP | KEY_RIGHT)) {
            diag_ur = true;
            gfx_draw_text(176, 61, "PASS", COLOR_EMERALD, COLOR_CARD_BG, false);
        }
        if (!diag_ul && keys_all_down(KEY_UP | KEY_LEFT)) {
            diag_ul = true;
            gfx_draw_text(176, 52, "PASS", COLOR_EMERALD, COLOR_CARD_BG, false);
        }
        if (!diag_dr && keys_all_down(KEY_DOWN | KEY_RIGHT)) {
            diag_dr = true;
            gfx_draw_text(176, 79, "PASS", COLOR_EMERALD, COLOR_CARD_BG, false);
        }
        if (!diag_dl && keys_all_down(KEY_DOWN | KEY_LEFT)) {
            diag_dl = true;
            gfx_draw_text(176, 70, "PASS", COLOR_EMERALD, COLOR_CARD_BG, false);
        }

        /* Update counters when buttons are pressed */
        if (g_input.pressed & (KEY_A | KEY_B)) {
            gfx_draw_text_fmt(154, 100, COLOR_WHITE, COLOR_CARD_BG, false, "A:%02d B:%02d",
                              (int)(g_input.press_counts[BTN_IDX_A] % 100),
                              (int)(g_input.press_counts[BTN_IDX_B] % 100));
        }
        if (g_input.pressed & (KEY_L | KEY_R)) {
            gfx_draw_text_fmt(154, 110, COLOR_WHITE, COLOR_CARD_BG, false, "L:%02d R:%02d",
                              (int)(g_input.press_counts[BTN_IDX_L] % 100),
                              (int)(g_input.press_counts[BTN_IDX_R] % 100));
        }

        /* Pivot error check (opposite inputs pressed simultaneously) */
        bool opp_error = (keys_all_down(KEY_LEFT | KEY_RIGHT)) ||
                         (keys_all_down(KEY_UP | KEY_DOWN));
        if (opp_error != prev_opp_err) {
            prev_opp_err = opp_error;
            if (opp_error) {
                gfx_draw_rect(154, 30, 76, 10, COLOR_RED);
                gfx_draw_text(156, 31, "!PIVOT ERR!", COLOR_WHITE, COLOR_RED, true);
            } else {
                gfx_draw_rect(154, 30, 76, 10, COLOR_CARD_BG);
                gfx_draw_text(154, 31, "Pivot: OK  ", COLOR_EMERALD, COLOR_CARD_BG, false);
            }
        }

        /* Combo test */
        bool soft_reset = keys_all_down(KEY_A | KEY_B | KEY_START | KEY_SELECT);
        if (soft_reset != prev_soft_reset) {
            prev_soft_reset = soft_reset;
            if (soft_reset) {
                gfx_draw_text(154, 122, "SOFT RESET!", COLOR_AMBER, COLOR_CARD_BG, false);
            } else {
                gfx_draw_text(154, 122, "Combo: IDLE", COLOR_DARK_GRAY, COLOR_CARD_BG, false);
            }
        }
    }
}
