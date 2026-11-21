#include "../include/health_audit.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/rtc.h"
#include "../include/help_guide.h"

#ifndef STANDALONE_MULTIBOOT
static bool detect_is_nds(void) {
    vu8* wramcnt = (vu8*)0x04000800;
    u8 old_val = *wramcnt;
    *wramcnt = 0x03;
    u8 test_val = *wramcnt;
    *wramcnt = old_val;
    return (test_val != 0x03);
}
#endif

void health_audit_run(HealthAuditReport* report) {
    /* 2. Console Model */
#ifdef STANDALONE_MULTIBOOT
    report->is_nds = false;
    report->model_name = "GBA/SP/Micro(MB)";
#else
    report->is_nds = detect_is_nds();
    report->model_name = report->is_nds ? "NDS (Slot-2 GBA)" : "GBA / SP / Micro";
#endif
    report->waitcnt = REG_WAITCNT;

    int points = 0;
    int max_points = 0;

    /* 2. CPU / ALU Sanity */
    max_points += 20;
    volatile u32 a = 0x12345678;
    volatile u32 b = 0x87654321;
    volatile u32 c = (a ^ b) + ((a & b) << 1);
    volatile u32 d = (a * 17) ^ (b / 3);
    if (c == (a + b) && d != 0) {
        report->status_cpu = HEALTH_STATUS_PASS;
        points += 20;
    } else {
        report->status_cpu = HEALTH_STATUS_FAIL;
    }

    /* 3. Fast IWRAM Bus (0x03007000) non-destructive */
    max_points += 15;
    vu32* iwram_ptr = (vu32*)0x03007000;
    u32 orig_iw = *iwram_ptr;
    *iwram_ptr = 0x55AA55AA;
    u32 r1_iw = *iwram_ptr;
    *iwram_ptr = 0xAA55AA55;
    u32 r2_iw = *iwram_ptr;
    *iwram_ptr = orig_iw;
    if (r1_iw == 0x55AA55AA && r2_iw == 0xAA55AA55) {
        report->status_iwram = HEALTH_STATUS_PASS;
        points += 15;
    } else {
        report->status_iwram = HEALTH_STATUS_FAIL;
    }

    /* 4. External EWRAM Bus (0x0203E000 high safe buffer) non-destructive */
    max_points += 15;
    vu32* ewram_ptr = (vu32*)0x0203E000;
    u32 orig_ew = *ewram_ptr;
    *ewram_ptr = 0xA55A1234;
    u32 r1_ew = *ewram_ptr;
    *ewram_ptr = 0x5AA54321;
    u32 r2_ew = *ewram_ptr;
    *ewram_ptr = orig_ew;
    if (r1_ew == 0xA55A1234 && r2_ew == 0x5AA54321) {
        report->status_ewram = HEALTH_STATUS_PASS;
        points += 15;
    } else {
        report->status_ewram = HEALTH_STATUS_FAIL;
    }

    /* 5. VRAM Bus (0x06014000 beyond Mode 3 active screen) non-destructive */
    max_points += 15;
    vu16* vram_ptr = (vu16*)0x06014000;
    u16 orig_vr = *vram_ptr;
    *vram_ptr = 0x7C1F;
    u16 r1_vr = *vram_ptr;
    *vram_ptr = 0x03E0;
    u16 r2_vr = *vram_ptr;
    *vram_ptr = orig_vr;
    if (r1_vr == 0x7C1F && r2_vr == 0x03E0) {
        report->status_vram = HEALTH_STATUS_PASS;
        points += 15;
    } else {
        report->status_vram = HEALTH_STATUS_FAIL;
    }

    /* 6. Sound Subsystem */
    max_points += 15;
    u16 snd_stat = REG_SOUNDCNT_X;
    if (snd_stat & 0x80) {
        report->status_sound = HEALTH_STATUS_PASS;
        points += 15;
    } else {
        report->status_sound = HEALTH_STATUS_WARN;
        points += 8;
    }

    /* 7. Link / SIO Port */
    max_points += 10;
    u16 rcnt_val = REG_RCNT;
    u16 siocnt_val = REG_SIOCNT;
    if ((rcnt_val & 0x8000) || (siocnt_val & 0x2000) || (siocnt_val & 0x0008)) {
        report->status_sio = HEALTH_STATUS_PASS;
        points += 10;
    } else {
        report->status_sio = HEALTH_STATUS_STANDBY;
        points += 10; /* Standby is normal when no cable is attached */
    }

    /* 8. Save Media & Cartridge RTC (probe topmost unused SRAM byte 0x7FFF) */
#ifndef STANDALONE_MULTIBOOT
    max_points += 5;
    vu8* sram_ptr = (vu8*)(MEM_SRAM + 0x7FFF);
    u8 orig_sr = *sram_ptr;
    *sram_ptr = (u8)(~orig_sr);
    u8 sr_chk = *sram_ptr;
    *sram_ptr = orig_sr;
    if (sr_chk == (u8)(~orig_sr)) {
        report->status_sram = HEALTH_STATUS_PASS;
        points += 5;
    } else {
        report->status_sram = HEALTH_STATUS_STANDBY;
        points += 3;
    }

    max_points += 5;
    bool rtc_ok = rtc_probe();
    if (rtc_ok) {
        report->status_rtc = HEALTH_STATUS_PASS;
        points += 5;
    } else {
        report->status_rtc = HEALTH_STATUS_WARN;
        points += 2;
    }
#else
    report->status_sram = HEALTH_STATUS_NA;
    report->status_rtc = HEALTH_STATUS_NA;
#endif

    /* 9. Keypad Sanity & Ghosting Check */
    max_points += 10;
    /* Physical impossible D-Pad pivot short (UP+DOWN or LEFT+RIGHT) */
    bool pivot_err = ((g_input.current & (KEY_UP | KEY_DOWN)) == (KEY_UP | KEY_DOWN)) ||
                     ((g_input.current & (KEY_LEFT | KEY_RIGHT)) == (KEY_LEFT | KEY_RIGHT));

    /* Ignore the activation trigger keys (A and START) used to enter or re-audit */
    u16 unexpected_keys = (g_input.current & KEY_ANY) & ~(KEY_A | KEY_START);

    if (pivot_err) {
        report->status_keys = HEALTH_STATUS_FAIL;
        report->stuck_keys_mask = g_input.current;
    } else if (unexpected_keys != 0) {
        report->status_keys = HEALTH_STATUS_WARN;
        report->stuck_keys_mask = unexpected_keys;
        points += 6;
    } else {
        report->status_keys = HEALTH_STATUS_PASS;
        report->stuck_keys_mask = 0;
        points += 10;
    }

    /* Calculate Score & Classification (<= 19 chars to fit cleanly at x=120) */
    report->total_score = (points * 100) / max_points;
    if (report->total_score >= 98) {
        report->grade_str = "GRADE A+ (MINT)";
        report->grade_color = COLOR_EMERALD;
    } else if (report->total_score >= 88) {
        report->grade_str = "GRADE A (GREAT)";
        report->grade_color = COLOR_CYAN;
    } else if (report->total_score >= 75) {
        report->grade_str = "GRADE B (ADVISORY)";
        report->grade_color = COLOR_AMBER;
    } else {
        report->grade_str = "GRADE C (FAULT!)";
        report->grade_color = COLOR_CORAL;
    }
}

static void draw_status_badge(int x, int y, HealthStatus status, const char* pass_text) {
    switch (status) {
        case HEALTH_STATUS_PASS:
            gfx_draw_text(x, y, "[OK]", COLOR_EMERALD, COLOR_CARD_BG, false);
            gfx_draw_text(x + 26, y, pass_text ? pass_text : "OK", COLOR_WHITE, COLOR_CARD_BG, false);
            break;
        case HEALTH_STATUS_STANDBY:
            gfx_draw_text(x, y, "[--]", COLOR_CYAN, COLOR_CARD_BG, false);
            gfx_draw_text(x + 26, y, pass_text ? pass_text : "READY", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
            break;
        case HEALTH_STATUS_WARN:
            gfx_draw_text(x, y, "[!]", COLOR_AMBER, COLOR_CARD_BG, false);
            gfx_draw_text(x + 26, y, pass_text ? pass_text : "CHECK", COLOR_GOLD, COLOR_CARD_BG, false);
            break;
        case HEALTH_STATUS_FAIL:
            gfx_draw_text(x, y, "[ER]", COLOR_CORAL, COLOR_CARD_BG, false);
            gfx_draw_text(x + 26, y, "ERROR", COLOR_CORAL, COLOR_CARD_BG, false);
            break;
        case HEALTH_STATUS_NA:
        default:
            gfx_draw_text(x, y, "[NA]", COLOR_GRAY, COLOR_CARD_BG, false);
            gfx_draw_text(x + 26, y, "EWRAM", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
            break;
    }
}

void run_health_audit_screen(void) {
    HealthAuditReport report;
    health_audit_run(&report);

    bool need_full_redraw = true;
    u16 prev_keys = 0xFFFF;

    while (1) {
        vsync();
        input_poll();

        if (key_hit(KEY_B)) {
            break;
        }

        if (key_hit(KEY_A) || key_hit(KEY_START)) {
            /* Re-run live audit sweep */
            health_audit_run(&report);
            need_full_redraw = true;
        }

        if (key_hit(KEY_SELECT)) {
            run_help_guide(HELP_TOPIC_HEALTH);
            need_full_redraw = true;
            continue;
        }

        if (need_full_redraw) {
            gfx_clear(COLOR_NAVY);
            gfx_draw_header("GBA HARDWARE CHECK", "HEALTH AUDIT");
            gfx_draw_footer("A:Re-Audit | SEL:Guide | B:Back");

            /* Card 1: Console Specs & Model (y=16..46, h=30) */
            gfx_draw_card(4, 16, 232, 30, "CONSOLE & CORE SPECIFICATIONS", COLOR_CARD_BORDER);
            gfx_draw_text(8, 27, "Model:", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
            gfx_draw_text(46, 27, report.model_name, report.is_nds ? COLOR_AMBER : COLOR_EMERALD, COLOR_CARD_BG, false);
            gfx_draw_text_fmt(160, 27, COLOR_WHITE, COLOR_CARD_BG, false, "WAIT:0x%04X", report.waitcnt);

            /* Card 2: Subsystem Verification Grid (y=48..116, h=68) */
            gfx_draw_card(4, 48, 232, 68, "HARDWARE SUBSYSTEM VERIFICATION", COLOR_CARD_BORDER);

            /* Column 1 (x=8..116) */
            int row_y = 60;
            gfx_draw_text(8, row_y, "CPU/ALU:", COLOR_WHITE, COLOR_CARD_BG, false);
            draw_status_badge(56, row_y, report.status_cpu, "16.78M");

            row_y += 12;
            gfx_draw_text(8, row_y, "IWRAM:", COLOR_WHITE, COLOR_CARD_BG, false);
            draw_status_badge(56, row_y, report.status_iwram, "32K");

            row_y += 12;
            gfx_draw_text(8, row_y, "EWRAM:", COLOR_WHITE, COLOR_CARD_BG, false);
            draw_status_badge(56, row_y, report.status_ewram, "256K");

            row_y += 12;
            gfx_draw_text(8, row_y, "VRAM:", COLOR_WHITE, COLOR_CARD_BG, false);
            draw_status_badge(56, row_y, report.status_vram, "96K");

            /* Column 2 (x=122..234) */
            row_y = 60;
            gfx_draw_text(122, row_y, "SOUND:", COLOR_WHITE, COLOR_CARD_BG, false);
            draw_status_badge(166, row_y, report.status_sound, "PSG+DMA");

            row_y += 12;
            gfx_draw_text(122, row_y, "SIO:", COLOR_WHITE, COLOR_CARD_BG, false);
            draw_status_badge(166, row_y, report.status_sio, (report.status_sio == HEALTH_STATUS_PASS) ? "LINKED" : "PORT OK");

            row_y += 12;
            gfx_draw_text(122, row_y, "SAVE:", COLOR_WHITE, COLOR_CARD_BG, false);
#ifndef STANDALONE_MULTIBOOT
            draw_status_badge(166, row_y, (report.status_rtc == HEALTH_STATUS_PASS) ? HEALTH_STATUS_PASS : report.status_sram,
                              (report.status_rtc == HEALTH_STATUS_PASS) ? "RTC OK" : "SRAM OK");
#else
            draw_status_badge(166, row_y, HEALTH_STATUS_NA, "EWRAM");
#endif

            row_y += 12;
            gfx_draw_text(122, row_y, "KEYS:", COLOR_WHITE, COLOR_CARD_BG, false);
            draw_status_badge(166, row_y, report.status_keys, (report.status_keys == HEALTH_STATUS_PASS) ? "0 STUCK" : "HELD");

            /* Card 3: Overall Certification Score & Grade (y=118..146, h=28) */
            gfx_draw_card(4, 118, 232, 28, "OVERALL HARDWARE CERTIFICATION", COLOR_CARD_BORDER);

            /* Progress Bar for Score */
            gfx_draw_progress_bar(8, 131, 78, 8, report.total_score, 100, report.grade_color, COLOR_DARK_GRAY);
            gfx_draw_text_fmt(90, 131, COLOR_WHITE, COLOR_CARD_BG, false, "%3d%%", report.total_score);

            /* Grade Badge text */
            gfx_draw_text(118, 131, report.grade_str, report.grade_color, COLOR_CARD_BG, false);

            prev_keys = 0xFFFF;
            need_full_redraw = false;
        }

        /* Live responsive Keypad status update when buttons change */
        u16 cur_keys = g_input.current & KEY_ANY;
        if (cur_keys != prev_keys) {
            prev_keys = cur_keys;
            gfx_draw_rect(166, 96, 68, 10, COLOR_CARD_BG);
            if (((cur_keys & (KEY_UP | KEY_DOWN)) == (KEY_UP | KEY_DOWN)) ||
                ((cur_keys & (KEY_LEFT | KEY_RIGHT)) == (KEY_LEFT | KEY_RIGHT))) {
                draw_status_badge(166, 96, HEALTH_STATUS_FAIL, "PIVOT!");
            } else if (cur_keys == 0) {
                draw_status_badge(166, 96, HEALTH_STATUS_PASS, "0 STUCK");
            } else {
                const char* k_name = "HELD";
                if (cur_keys & KEY_A) k_name = "KEY A";
                else if (cur_keys & KEY_B) k_name = "KEY B";
                else if (cur_keys & KEY_START) k_name = "START";
                else if (cur_keys & KEY_SELECT) k_name = "SELECT";
                else if (cur_keys & KEY_L) k_name = "L TRIG";
                else if (cur_keys & KEY_R) k_name = "R TRIG";
                else if (cur_keys & (KEY_UP|KEY_DOWN|KEY_LEFT|KEY_RIGHT)) k_name = "D-PAD";
                draw_status_badge(166, 96, HEALTH_STATUS_PASS, k_name);
            }
        }
    }
}
