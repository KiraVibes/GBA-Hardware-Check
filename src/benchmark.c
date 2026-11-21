#include "../include/benchmark.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/help_guide.h"

static void timer_start(void) {
    /* Stop timers */
    REG_TM0CNT_H = 0;
    REG_TM1CNT_H = 0;
    /* Reset counters */
    REG_TM0CNT_L = 0;
    REG_TM1CNT_L = 0;
    /* Timer 0: 16.78MHz (no prescaler), Timer 1: Countup / Cascade */
    REG_TM0CNT_H = 0x0080;
    REG_TM1CNT_H = 0x0084;
}

static u32 timer_stop(void) {
    REG_TM0CNT_H = 0;
    REG_TM1CNT_H = 0;
    u16 low = REG_TM0CNT_L;
    u16 high = REG_TM1CNT_L;
    return ((u32)high << 16) | (u32)low;
}

/* 16KB test buffer in EWRAM */
static u16 ewram_test_buf[8192] __attribute__((section(".bss")));

static void display_results(u32 ewram_wr_cycles, u32 ewram_rd_cycles, u32 rom_rd_cycles,
                            u32 cpu_math_cycles, u32 frame_cycles, bool pattern_test_pass) {
    gfx_clear(COLOR_NAVY);
    gfx_draw_header("BENCHMARK SUITE", "COMPLETE");
    gfx_draw_footer("A:Re-run | SEL:Guide | B:Back");

    /* Card 1: Memory Bandwidth */
    gfx_draw_card(4, 18, 232, 44, "MEMORY BANDWIDTH (16 KB)", COLOR_CARD_BORDER);

    /* Exact speed formula: (16384 bytes * 16777216 Hz * 10) / (cycles * 1048576) = 2621440000 / cycles */
    u32 ew_wr_mb = ewram_wr_cycles ? (2621440000u / ewram_wr_cycles) : 0;
    u32 ew_rd_mb = ewram_rd_cycles ? (2621440000u / ewram_rd_cycles) : 0;
    u32 rom_rd_mb = rom_rd_cycles ? (2621440000u / rom_rd_cycles) : 0;

    /* Nominal specs: EWRAM >= 2.8 MB/s, ROM >= 2.5 MB/s, CPU 100k-520k cyc (ROM/PSRAM bus: ~460k cyc, IWRAM: ~140k cyc), Frame 278k-284k cyc */
    bool ew_wr_ok = (ew_wr_mb >= 28);
    bool ew_rd_ok = (ew_rd_mb >= 28);
    bool rom_rd_ok = (rom_rd_mb >= 25);
#ifdef STANDALONE_MULTIBOOT
    /* EWRAM execution: no prefetch, 16-bit 2-waitstate bus (6 cyc/32-bit op) -> nominal ~1.05M cyc */
    bool cpu_ok = (cpu_math_cycles >= 750000 && cpu_math_cycles <= 1350000);
#else
    bool cpu_ok = (cpu_math_cycles >= 100000 && cpu_math_cycles <= 520000);
#endif
    bool frame_ok = (frame_cycles >= 278000 && frame_cycles <= 284000);

    u16 wr_col = ew_wr_ok ? COLOR_EMERALD : COLOR_RED;
    gfx_draw_text(8, 30, "EWRAM Wr:", COLOR_WHITE, COLOR_CARD_BG, false);
    gfx_draw_text_fmt(64, 30, wr_col, COLOR_CARD_BG, false,
                      "%u cyc (%u.%u MB/s) %s",
                      ewram_wr_cycles, ew_wr_mb / 10, ew_wr_mb % 10,
                      ew_wr_ok ? "[OK]" : "[LO]");

    u16 rd_col = ew_rd_ok ? COLOR_EMERALD : COLOR_RED;
    gfx_draw_text(8, 41, "EWRAM Rd:", COLOR_WHITE, COLOR_CARD_BG, false);
    gfx_draw_text_fmt(64, 41, rd_col, COLOR_CARD_BG, false,
                      "%u cyc (%u.%u MB/s) %s",
                      ewram_rd_cycles, ew_rd_mb / 10, ew_rd_mb % 10,
                      ew_rd_ok ? "[OK]" : "[LO]");

    u16 rom_col = rom_rd_ok ? COLOR_EMERALD : COLOR_RED;
#ifdef STANDALONE_MULTIBOOT
    gfx_draw_text(8, 52, "IWRAM Rd:", COLOR_WHITE, COLOR_CARD_BG, false);
#else
    gfx_draw_text(8, 52, "ROM Rd:  ", COLOR_WHITE, COLOR_CARD_BG, false);
#endif
    gfx_draw_text_fmt(64, 52, rom_col, COLOR_CARD_BG, false,
                      "%u cyc (%u.%u MB/s) %s",
                      rom_rd_cycles, rom_rd_mb / 10, rom_rd_mb % 10,
                      rom_rd_ok ? "[OK]" : "[LO]");

    /* Card 2: CPU Throughput */
    gfx_draw_card(4, 66, 232, 40, "CPU THROUGHPUT & TIMING", COLOR_CARD_BORDER);
    u16 cpu_col = cpu_ok ? COLOR_EMERALD : COLOR_RED;
#ifdef STANDALONE_MULTIBOOT
    const char* bus_info = "(EWRAM) [OK]";
#else
    const char* bus_info = (cpu_math_cycles > 260000) ? "(ROM) [OK]" : "(IWRAM) [OK]";
#endif
    gfx_draw_text(8, 78, "CPU ALU:", COLOR_WHITE, COLOR_CARD_BG, false);
    gfx_draw_text_fmt(64, 78, cpu_col, COLOR_CARD_BG, false,
                      "%u cyc %s", cpu_math_cycles, cpu_ok ? bus_info : "[SLOW]");

    u16 fr_col = frame_ok ? COLOR_EMERALD : COLOR_RED;
    gfx_draw_text(8, 90, "Frame:", COLOR_WHITE, COLOR_CARD_BG, false);
    gfx_draw_text_fmt(64, 90, fr_col, COLOR_CARD_BG, false,
                      "%u cyc (59.7Hz) %s", frame_cycles, frame_ok ? "[OK]" : "[DRFT]");

    /* Card 3: Memory Integrity Test */
    gfx_draw_card(4, 108, 232, 38, "RAM DATA BUS INTEGRITY", COLOR_CARD_BORDER);
    if (pattern_test_pass) {
        gfx_draw_text(8, 120, "Walking 1s & 0xAA55: PASS (100% OK)", COLOR_EMERALD, COLOR_CARD_BG, false);
        gfx_draw_text(8, 131, "All 16 data bus lines intact.", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
    } else {
        gfx_draw_text(8, 120, "RAM Integrity Pattern: FAILED!", COLOR_RED, COLOR_CARD_BG, false);
        gfx_draw_text(8, 131, "Check RAM chip / solder joints!", COLOR_CORAL, COLOR_CARD_BG, false);
    }
}

void run_benchmark_screen(void) {
    bool run_bench = true;
    u32 frame_cycles = 0;
    u32 ewram_wr_cycles = 0;
    u32 ewram_rd_cycles = 0;
    u32 rom_rd_cycles = 0;
    u32 cpu_math_cycles = 0;
    bool pattern_test_pass = false;

    while (1) {
        if (run_bench) {
            gfx_clear(COLOR_NAVY);
            gfx_draw_header("BENCHMARK SUITE", "TESTING");
            gfx_draw_footer("Please wait, measuring hardware...");
            gfx_draw_text(20, 75, "CALIBRATING HARDWARE TIMINGS...", COLOR_AMBER, COLOR_NAVY, false);

            /* 1. Frame cycles */
            vsync();
            timer_start();
            vsync();
            frame_cycles = timer_stop();

            /* 2. EWRAM Write */
            timer_start();
            for (int i = 0; i < 8192; i++) {
                ewram_test_buf[i] = (u16)i;
            }
            ewram_wr_cycles = timer_stop();

            /* 3. EWRAM Read */
            volatile u32 sum = 0;
            timer_start();
            for (int i = 0; i < 8192; i++) {
                sum += ewram_test_buf[i];
            }
            ewram_rd_cycles = timer_stop();

            /* 4. ROM Read / IWRAM Read */
#ifdef STANDALONE_MULTIBOOT
            const vu16* rom_ptr = (const vu16*)0x03000000;
#else
            const vu16* rom_ptr = (const vu16*)0x08000000;
#endif
            volatile u32 rom_sum = 0;
            timer_start();
            for (int i = 0; i < 8192; i++) {
                rom_sum += rom_ptr[i];
            }
            rom_rd_cycles = timer_stop();

            /* 5. CPU Math */
            volatile u32 math_acc = 12345;
            timer_start();
            for (int i = 0; i < 10000; i++) {
                math_acc = (math_acc * 1664525u + 1013904223u);
                math_acc ^= (math_acc >> 13);
            }
            cpu_math_cycles = timer_stop();

            /* 6. RAM integrity */
            pattern_test_pass = true;
            for (int i = 0; i < 16; i++) {
                u16 walking = (u16)(1 << i);
                ewram_test_buf[0] = walking;
                if (ewram_test_buf[0] != walking) { pattern_test_pass = false; break; }
            }
            for (int i = 0; i < 8192; i++) {
                u16 pattern = (i & 1) ? 0xAA55 : 0x55AA;
                ewram_test_buf[i] = pattern;
            }
            for (int i = 0; i < 8192; i++) {
                u16 expected = (i & 1) ? 0xAA55 : 0x55AA;
                if (ewram_test_buf[i] != expected) { pattern_test_pass = false; break; }
            }

            display_results(ewram_wr_cycles, ewram_rd_cycles, rom_rd_cycles,
                            cpu_math_cycles, frame_cycles, pattern_test_pass);
            run_bench = false;
        }

        vsync();
        input_poll();

        if (key_hit(KEY_B)) {
            break;
        }

        if (key_hit(KEY_A)) {
            run_bench = true;
        }

        if (key_hit(KEY_SELECT)) {
            run_help_guide(HELP_TOPIC_BENCHMARK);
            display_results(ewram_wr_cycles, ewram_rd_cycles, rom_rd_cycles,
                            cpu_math_cycles, frame_cycles, pattern_test_pass);
        }
    }
}
