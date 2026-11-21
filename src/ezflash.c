#include "../include/ezflash.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/rtc.h"
#include "../include/help_guide.h"

static const char* rumble_levels[4] = {
    "OFF (0% Power)",
    "PULSE (12f ON / 8f OFF)",
    "FLUTTER (20f ON / 5f OFF)",
    "CONTINUOUS (100% Max)"
};

static void draw_ezflash_static_ui(void) {
    gfx_clear(COLOR_NAVY);
    gfx_draw_header("EZ-FLASH & RUMBLE", "HW CHECK");
    gfx_draw_footer("A:Burst | L:Tap | SEL:Guide | B:Back");

    /* Card 1: Flashcart & Battery Status */
    gfx_draw_card(4, 16, 232, 40, "CARTRIDGE & RTC BATTERY STATUS", COLOR_CARD_BORDER);
    gfx_draw_text(8, 28, "Cart: EZ-Flash DE / Drill Dozer GPIO", COLOR_WHITE, COLOR_CARD_BG, false);

    /* Card 2: Haptic Rumble Motor */
    gfx_draw_card(4, 59, 232, 42, "HAPTIC RUMBLE MOTOR TEST (GPIO PIN 3)", COLOR_CARD_BORDER);
    gfx_draw_text(8, 84, "A:Burst | L:Tap | UP/DN:Level | STA", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);

    /* Card 3: Hardware Protocols */
    gfx_draw_card(4, 104, 232, 42, "RUMBLE HARDWARE INTERFACES", COLOR_CARD_BORDER);
    gfx_draw_text(8, 116, "Protocol: Drill Dozer GPIO3 / EZODE", COLOR_WHITE, COLOR_CARD_BG, false);
    gfx_draw_text(8, 128, "ROM Code: V49E (Auto Rumble Trigger)", COLOR_CYAN, COLOR_CARD_BG, false);
}

static void draw_ezflash_rtc_line(const RtcTime* rtc, bool has_rtc) {
    gfx_draw_rect(8, 40, 224, 10, COLOR_CARD_BG);
    if (has_rtc && rtc->present) {
        gfx_draw_text_fmt(8, 40, COLOR_EMERALD, COLOR_CARD_BG, false,
                          "RTC LIVE: 20%02d-%02d-%02d %02d:%02d:%02d",
                          rtc->year, rtc->month, rtc->day,
                          rtc->hours, rtc->minutes, rtc->seconds);
    } else {
        gfx_draw_text(8, 40, "RTC: NOT DETECTED (Check CR1220)", COLOR_AMBER, COLOR_CARD_BG, false);
    }
}

void run_ezflash_screen(void) {
    int rumble_strength = 0; /* 0=OFF, 1=PULSE, 2=FLUTTER, 3=CONTINUOUS */
    int prev_rumble = -1;
    bool prev_burst = false;
    int pulse_frames = 0;
    int tick = 0;
    int prev_rtc_sec = -2;

    RtcTime rtc;
    bool has_rtc = rtc_read_time(&rtc);

    draw_ezflash_static_ui();
    draw_ezflash_rtc_line(&rtc, has_rtc);
    prev_rtc_sec = (has_rtc && rtc.present) ? rtc.seconds : -1;

    while (1) {
        vsync();
        input_poll();
        tick++;

        if (key_hit(KEY_B)) {
            rumble_set(false);
            break;
        }

        /* Rumble controls: UP/DOWN changes continuous level */
        if (key_hit(KEY_UP)) {
            rumble_strength = (rumble_strength + 1) % 4;
            pulse_frames = 0;
        }
        if (key_hit(KEY_DOWN)) {
            rumble_strength = (rumble_strength - 1 + 4) % 4;
            pulse_frames = 0;
        }

        /* START: Direct toggle between OFF and 100% CONTINUOUS */
        if (key_hit(KEY_START)) {
            rumble_strength = (rumble_strength == 3) ? 0 : 3;
            pulse_frames = 0;
        }

        /* SELECT: Open Hardware Reference Guide */
        if (key_hit(KEY_SELECT)) {
            rumble_set(false);
            run_help_guide(HELP_TOPIC_EZFLASH);
            has_rtc = rtc_read_time(&rtc);
            draw_ezflash_static_ui();
            draw_ezflash_rtc_line(&rtc, has_rtc);
            prev_rtc_sec = (has_rtc && rtc.present) ? rtc.seconds : -1;
            prev_rumble = -1;
            prev_burst = false;
            continue;
        }

        /* A: Trigger high-strength haptic burst for 30 frames (0.5s) */
        if (key_hit(KEY_A)) {
            pulse_frames = 30;
        }

        /* L: Quick sharp tap for 8 frames (~133ms) */
        if (key_hit(KEY_L)) {
            pulse_frames = 8;
        }

        /* Hardware Drive & Duty Cycle Handling */
        if (pulse_frames > 0) {
            rumble_set(true);
            pulse_frames--;
            if (pulse_frames == 0) {
                rumble_strength = 0;
                rumble_set(false);
                prev_rumble = -1; /* Force UI redraw to show OFF */
            }
        } else {
            bool active = false;
            if (rumble_strength == 1) {
                /* Level 1: Rhythmic Heartbeat Pulse (12 frames ON, 8 frames OFF) */
                active = ((tick % 20) < 12);
            } else if (rumble_strength == 2) {
                /* Level 2: Heavy Flutter Buzz (20 frames ON, 5 frames OFF) */
                active = ((tick % 25) < 20);
            } else if (rumble_strength == 3) {
                /* Level 3: 100% Continuous Full Power */
                active = true;
            }
            rumble_set(active);
        }

        /* Update live RTC line when seconds change */
        if ((tick & 15) == 0) {
            has_rtc = rtc_read_time(&rtc);
            if (has_rtc && rtc.present) {
                if (rtc.seconds != prev_rtc_sec) {
                    prev_rtc_sec = rtc.seconds;
                    draw_ezflash_rtc_line(&rtc, true);
                }
            } else if (prev_rtc_sec != -1) {
                prev_rtc_sec = -1;
                draw_ezflash_rtc_line(&rtc, false);
            }
        }

        /* Dirty update of Rumble Card (zero flicker during burst!) */
        bool in_burst = (pulse_frames > 0);
        if (rumble_strength != prev_rumble || in_burst != prev_burst) {
            prev_rumble = rumble_strength;
            prev_burst = in_burst;
            gfx_draw_rect(8, 71, 224, 11, COLOR_CARD_BG);
            if (in_burst) {
                gfx_draw_text(8, 72, "Motor: >> HAPTIC BURST ACTIVE! <<", COLOR_CORAL, COLOR_CARD_BG, false);
            } else {
                u16 r_col = (rumble_strength > 0) ? COLOR_EMERALD : COLOR_LIGHT_GRAY;
                gfx_draw_text_fmt(8, 72, r_col, COLOR_CARD_BG, false,
                                  "Level: %s", rumble_levels[rumble_strength]);
            }
        }
    }

    rumble_set(false);
}
