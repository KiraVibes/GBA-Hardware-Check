#include "../include/sound.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/help_guide.h"
#include "../include/directsound_data.h"
#ifndef STANDALONE_MULTIBOOT
#include "../include/midi_player.h"
#endif
#include "../include/localization.h"

static s8 dsa_ewram_buffer[DIRECTSOUND_SAMPLE_COUNT] __attribute__((aligned(4)));
static s8 dsb_ewram_buffer[DIRECTSOUND_SAMPLE_COUNT] __attribute__((aligned(4)));

void sound_init(void) {
    /* Enable Master Sound Controller */
    REG_SOUNDCNT_X = 0x0080;
    /* Master Volume 7/7 on both Left and Right, enable all 4 channels to both L & R */
    REG_SOUNDCNT_L = 0xFF77;
    /* PSG Master Volume 100% (Bits 0-1 = 2: 100%, 3 is prohibited/invalid) */
    REG_SOUNDCNT_H = 0x0002;
    sound_stop_all();

    /* Clear DirectSound EWRAM buffers to clean silence */
    u32* a = (u32*)dsa_ewram_buffer;
    u32* b = (u32*)dsb_ewram_buffer;
    for (int i = 0; i < DIRECTSOUND_SAMPLE_COUNT / 4; i++) {
        a[i] = 0;
        b[i] = 0;
    }
}

void sound_stop_directsound_a(void) {
    REG_DMA1CNT_H = 0;
    REG_TM0CNT_H = 0;
    REG_SOUNDCNT_H &= ~( (1 << 2) | (1 << 8) | (1 << 9) );
    /* Pulse FIFO A reset to flush any pending buffer samples */
    REG_SOUNDCNT_H |= (1 << 11);
    REG_SOUNDCNT_H &= ~(1 << 11);
}

void sound_stop_directsound_b(void) {
    REG_DMA2CNT_H = 0;
    REG_TM1CNT_H = 0;
    REG_SOUNDCNT_H &= ~( (1 << 3) | (1 << 12) | (1 << 13) );
    /* Pulse FIFO B reset to flush any pending buffer samples */
    REG_SOUNDCNT_H |= (1 << 15);
    REG_SOUNDCNT_H &= ~(1 << 15);
}

void sound_stop_directsound(void) {
    sound_stop_directsound_a();
    sound_stop_directsound_b();
}

void sound_stop_all(void) {
    sound_stop_directsound();

    /* Mute PSG voices with volume 0 re-triggers to prevent residual DC hum */
    REG_SOUND1CNT_H = 0;
    REG_SOUND1CNT_X = 0x8000;
    REG_SOUND2CNT_L = 0;
    REG_SOUND2CNT_H = 0x8000;
    REG_SOUND3CNT_H = 0x0000;
    REG_SOUND3CNT_L = 0x0000;
    REG_SOUND4CNT_L = 0;
    REG_SOUND4CNT_H = 0x8000;
}

void sound_set_pan(int channel, u8 pan) {
    if (channel >= 1 && channel <= 4) {
        u16 l = REG_SOUNDCNT_L & 0x0077;
        u16 pan_mask = 0;

        int r_bit = 8 + (channel - 1);
        int l_bit = 12 + (channel - 1);

        if (pan & PAN_RIGHT) pan_mask |= (u16)(1 << r_bit);
        if (pan & PAN_LEFT)  pan_mask |= (u16)(1 << l_bit);

        u16 other_mask = (u16)(0xFF00 & ~((1 << r_bit) | (1 << l_bit)));
        REG_SOUNDCNT_L = (REG_SOUNDCNT_L & other_mask) | pan_mask | l;
    } else if (channel == 5) {
        /* DirectSound A (REG_SOUNDCNT_H bits 8 and 9) */
        u16 h = REG_SOUNDCNT_H & ~((1 << 8) | (1 << 9));
        if (pan & PAN_RIGHT) h |= (1 << 8);
        if (pan & PAN_LEFT)  h |= (1 << 9);
        REG_SOUNDCNT_H = h;
    } else if (channel == 6) {
        /* DirectSound B (REG_SOUNDCNT_H bits 12 and 13) */
        u16 h = REG_SOUNDCNT_H & ~((1 << 12) | (1 << 13));
        if (pan & PAN_RIGHT) h |= (1 << 12);
        if (pan & PAN_LEFT)  h |= (1 << 13);
        REG_SOUNDCNT_H = h;
    }
}

void sound_play_square1(u16 freq_val, u8 duty, u8 sweep_shift, bool sweep_down, u8 pan) {
    sound_set_pan(1, pan);

    /* 1. Reset channel 1 so hardware sweep unit resets cleanly */
    REG_SOUND1CNT_H = 0;
    REG_SOUND1CNT_X = 0;

    /* 2. Configure sweep unit */
    u16 sweep = 0;
    if (sweep_shift > 0) {
        sweep = (u16)((sweep_shift & 7) | (sweep_down ? (1 << 3) : 0) | (3 << 4));
    }
    REG_SOUND1CNT_L = sweep;

    /* 3. Duty cycle and volume envelope (initial vol 15, decay step 2) */
    u16 duty_env = (u16)(((duty & 3) << 6) | (15 << 12) | (2 << 8));
    REG_SOUND1CNT_H = duty_env;

    /* 4. Trigger */
    REG_SOUND1CNT_X = (u16)(0x8000 | (freq_val & 0x07FF));
}

void sound_play_square2(u16 freq_val, u8 duty, u8 pan) {
    sound_set_pan(2, pan);

    u16 duty_env = (u16)(((duty & 3) << 6) | (15 << 12) | (2 << 8));
    REG_SOUND2CNT_L = duty_env;
    REG_SOUND2CNT_H = (u16)(0x8000 | (freq_val & 0x07FF));
}

void sound_play_wave(u16 freq_val, int waveform_type, u8 pan) {
    sound_set_pan(3, pan);

    /* 1. Stop Channel 3 playback before modifying Wave RAM */
    REG_SOUND3CNT_L = 0x0000;
    REG_SOUND3CNT_X = 0x0000;

    /* 2. 32 4-bit samples packed into 8 16-bit halfwords */
    static const u16 sine_wave[8] = {
        0xDF8B, 0xECFF, 0x64A8, 0x0021,
        0x2401, 0xAC68, 0xFFEF, 0x98DB
    };
    static const u16 tri_wave[8] = {
        0x6745, 0x2301, 0xEFCD, 0xAB89,
        0x98BA, 0xDCFE, 0x1032, 0x5476
    };
    const u16* wave = (waveform_type == 0) ? sine_wave : tri_wave;

    vu16* wave_ram = (vu16*)(MEM_IO + 0x0090);

    /* Write to Bank 0 (select Bank 1 for playback so CPU writes target Bank 0) */
    REG_SOUND3CNT_L = 0x0040;
    for (int i = 0; i < 8; i++) {
        wave_ram[i] = wave[i];
    }

    /* Write to Bank 1 (select Bank 0 for playback so CPU writes target Bank 1) */
    REG_SOUND3CNT_L = 0x0000;
    for (int i = 0; i < 8; i++) {
        wave_ram[i] = wave[i];
    }

    /* 3. Enable Channel 3 playback from Bank 0, 32-sample mode (Bit 7 = 1) */
    REG_SOUND3CNT_L = 0x0080;

    /* 4. Set Volume to 100% (Bits 13-14 = 1) */
    REG_SOUND3CNT_H = 0x2000;

    /* 5. Trigger sound playback (Bit 15 = 1, Bit 14 = 0 continuous) */
    REG_SOUND3CNT_X = (u16)(0x8000 | (freq_val & 0x07FF));
}

void sound_play_noise(u8 clock_div, bool seven_step, u8 pan) {
    sound_set_pan(4, pan);

    REG_SOUND4CNT_L = (15 << 12) | (2 << 8);
    u16 poly = (u16)((clock_div & 7) | (seven_step ? (1 << 3) : 0) | (4 << 4));
    REG_SOUND4CNT_H = (u16)(0x8000 | poly);
}



void sound_play_directsound_a_note(const s8* samples, int count, u16 timer_reload, u8 pan, u8 vol) {
    if (vol == 0) {
        sound_stop_directsound_a();
        return;
    }

    /* 1. Stop previous DMA1 and Timer 0 */
    REG_DMA1CNT_H = 0;
    REG_TM0CNT_H = 0;

    int total_samples = count;
    if (total_samples > DIRECTSOUND_SAMPLE_COUNT) total_samples = DIRECTSOUND_SAMPLE_COUNT;

    if (vol >= 100) {
        int words = total_samples / 4;
        const u32* s32 = (const u32*)samples;
        u32* d32 = (u32*)dsa_ewram_buffer;
        for (int i = 0; i < words; i++) {
            d32[i] = s32[i];
        }
        int max_words = DIRECTSOUND_SAMPLE_COUNT / 4;
        int zero_end = words + 512;
        if (zero_end > max_words) zero_end = max_words;
        for (int i = words; i < zero_end; i++) {
            d32[i] = 0;
        }
    } else {
        s8* dst = dsa_ewram_buffer;
        for (int i = 0; i < total_samples; i++) {
            dst[i] = (s8)(((int)samples[i] * vol) / 100);
        }
        int zero_end = total_samples + 2048;
        if (zero_end > DIRECTSOUND_SAMPLE_COUNT) zero_end = DIRECTSOUND_SAMPLE_COUNT;
        for (int i = total_samples; i < zero_end; i++) {
            dst[i] = 0;
        }
    }

    /* Preserve PSG (bits 0-1), DSB vol (bit 3), DSB routing (bits 12-14) */
    u16 dsa_ctrl = (REG_SOUNDCNT_H & 0x700B) | (1 << 2);
    if (pan & PAN_RIGHT) dsa_ctrl |= (1 << 8);
    if (pan & PAN_LEFT)  dsa_ctrl |= (1 << 9);

    /* 2. Pulse FIFO Reset to empty FIFO BEFORE priming and starting DMA */
    REG_SOUNDCNT_H = dsa_ctrl | (1 << 11);
    REG_SOUNDCNT_H = dsa_ctrl;

    /* 3. Prime FIFO A with the first 16 bytes (4 32-bit words) */
    vu32* fifo = (vu32*)0x040000A0;
    const u32* src = (const u32*)dsa_ewram_buffer;
    for (int i = 0; i < 4; i++) {
        *fifo = src[i];
    }

    /* 4. Configure DMA1 to start from byte 16 */
    REG_DMA1SAD = (u32)(dsa_ewram_buffer + 16);
    REG_DMA1DAD = 0x040000A0;
    REG_DMA1CNT_L = 0;
    REG_DMA1CNT_H = 0xB640;

    /* 5. Start Timer 0 */
    REG_TM0CNT_L = timer_reload;
    REG_TM0CNT_H = 0x0080;
}

void sound_play_directsound_a(const s8* samples, int count, u8 pan, u8 vol) {
    sound_play_directsound_a_note(samples, count, 0xFC00, pan, vol);
}

void sound_play_directsound_b_note(const s8* samples, int count, u16 timer_reload, u8 pan, u8 vol) {
    if (vol == 0) {
        sound_stop_directsound_b();
        return;
    }

    /* 1. Stop previous DMA2 and Timer 1 */
    REG_DMA2CNT_H = 0;
    REG_TM1CNT_H = 0;

    int total_samples = count;
    if (total_samples > DIRECTSOUND_SAMPLE_COUNT) total_samples = DIRECTSOUND_SAMPLE_COUNT;

    if (vol >= 100) {
        int words = total_samples / 4;
        const u32* s32 = (const u32*)samples;
        u32* d32 = (u32*)dsb_ewram_buffer;
        for (int i = 0; i < words; i++) {
            d32[i] = s32[i];
        }
        int max_words = DIRECTSOUND_SAMPLE_COUNT / 4;
        int zero_end = words + 512;
        if (zero_end > max_words) zero_end = max_words;
        for (int i = words; i < zero_end; i++) {
            d32[i] = 0;
        }
    } else {
        s8* dst = dsb_ewram_buffer;
        for (int i = 0; i < total_samples; i++) {
            dst[i] = (s8)(((int)samples[i] * vol) / 100);
        }
        int zero_end = total_samples + 2048;
        if (zero_end > DIRECTSOUND_SAMPLE_COUNT) zero_end = DIRECTSOUND_SAMPLE_COUNT;
        for (int i = total_samples; i < zero_end; i++) {
            dst[i] = 0;
        }
    }

    /* Preserve PSG (bits 0-1), DSA vol (bit 2), DSA routing (bits 8-10) */
    u16 dsb_ctrl = (REG_SOUNDCNT_H & 0x0707) | (1 << 3) | (1 << 14); /* Timer 1 select */
    if (pan & PAN_RIGHT) dsb_ctrl |= (1 << 12);
    if (pan & PAN_LEFT)  dsb_ctrl |= (1 << 13);

    /* 2. Pulse FIFO B Reset to empty FIFO BEFORE priming and starting DMA */
    REG_SOUNDCNT_H = dsb_ctrl | (1 << 15);
    REG_SOUNDCNT_H = dsb_ctrl;

    /* 3. Prime FIFO B with the first 16 bytes (4 32-bit words) */
    vu32* fifo = (vu32*)0x040000A4;
    const u32* src = (const u32*)dsb_ewram_buffer;
    for (int i = 0; i < 4; i++) {
        *fifo = src[i];
    }

    /* 4. Configure DMA2 to start from byte 16 */
    REG_DMA2SAD = (u32)(dsb_ewram_buffer + 16);
    REG_DMA2DAD = 0x040000A4;
    REG_DMA2CNT_L = 0;
    REG_DMA2CNT_H = 0xB640;

    /* 5. Start Timer 1 */
    REG_TM1CNT_L = timer_reload;
    REG_TM1CNT_H = 0x0080;
}

void sound_play_directsound_b(const s8* samples, int count, u8 pan, u8 vol) {
    sound_play_directsound_b_note(samples, count, 0xFC00, pan, vol);
}

#ifdef STANDALONE_MULTIBOOT
#define ITEM_COUNT 8
static const char* items[LANG_COUNT][ITEM_COUNT] = {
    /* LANG_DE */
    {
        "1. Rechteck 1 Sweep-Effekt (FX)",
        "2. Rechteck 2 Lead-Ton (440Hz)",
        "3. Wave-RAM Synth (Sinuswelle)",
        "4. Rauschgenerator (Drums/Snare)",
        "5. DirectSound 8-Bit PCM (DMA1)",
        "6. Stereotrennung & Balance (L/R)",
        "7. Chromatische Test-Tonleiter",
        "8. Resonanz & Frequenz-Sweep"
    },
    /* LANG_EN */
    {
        "1. Square 1 + Pitch Sweep (FX)",
        "2. Square 2 Melodic Lead (440Hz)",
        "3. Wave RAM Synth (Smooth Sine)",
        "4. Noise Drum (Snare/Percussion)",
        "5. DirectSound 8-bit PCM (DMA1)",
        "6. Stereo Separation Test (L/R)",
        "7. Chromatic Test Scale",
        "8. Speaker Sweep & Resonance"
    }
};
#else
#define ITEM_COUNT 9
static const char* items[LANG_COUNT][ITEM_COUNT] = {
    /* LANG_DE */
    {
        "1. Rechteck 1 Sweep-Effekt (FX)",
        "2. Rechteck 2 Lead-Ton (440Hz)",
        "3. Wave-RAM Synth (Sinuswelle)",
        "4. Rauschgenerator (Drums/Snare)",
        "5. DirectSound 8-Bit PCM (DMA1)",
        "6. Stereotrennung & Balance (L/R)",
        "7. Chromatische Test-Tonleiter",
        "8. Resonanz & Frequenz-Sweep",
        "9. MIDI-Player & Sound-Profile"
    },
    /* LANG_EN */
    {
        "1. Square 1 + Pitch Sweep (FX)",
        "2. Square 2 Melodic Lead (440Hz)",
        "3. Wave RAM Synth (Smooth Sine)",
        "4. Noise Drum (Snare/Percussion)",
        "5. DirectSound 8-bit PCM (DMA1)",
        "6. Stereo Separation Test (L/R)",
        "7. Chromatic Test Scale",
        "8. Speaker Sweep & Resonance",
        "9. MIDI Sequencer (12 Songs / 6-Ch)"
    }
};
#endif

static u16 sweep_hz_to_psg(int hz) {
    if (hz < 64) hz = 64;
    if (hz > 20000) hz = 20000;
    int n = 2048 - (131072 / hz);
    if (n < 0) n = 0;
    if (n > 2047) n = 2047;
    return (u16)n;
}

static u16 sweep_hz_to_wave(int hz) {
    if (hz < 64) hz = 64;
    if (hz > 20000) hz = 20000;
    int n = 2048 - (65536 / hz);
    if (n < 0) n = 0;
    if (n > 2047) n = 2047;
    return (u16)n;
}

static void run_audio_sweep_screen(void) {
    sound_stop_all();
    sound_init();

    int cur_hz = 440;
    int prev_hz = -1;
    int channel_mode = 0; /* 0: Square 2, 1: Wave RAM Sine */
    int prev_channel_mode = -1;
    bool auto_sweep = false;
    bool prev_auto_sweep = true;
    int prev_hint_band = -1;
    bool tone_needs_trigger = true;
    int tick = 0;
    bool need_redraw = true;
    Language prev_lang = g_lang;

    while (1) {
        vsync();
        input_poll();
        tick++;

        if (key_hit(KEY_B)) {
            sound_stop_all();
            break;
        }

        if (key_hit(KEY_SELECT)) {
            sound_stop_all();
            run_help_guide(HELP_TOPIC_AUDIO);
            sound_init();
            need_redraw = true;
            tone_needs_trigger = true;
            continue;
        }

        /* Toggle Auto-Sweep with A */
        if (key_hit(KEY_A)) {
            auto_sweep = !auto_sweep;
        }

        /* Switch channel with L / R */
        if (key_hit(KEY_L)) {
            channel_mode = (channel_mode - 1 + 2) % 2;
            sound_stop_all();
            sound_init();
            tone_needs_trigger = true;
        }
        if (key_hit(KEY_R)) {
            channel_mode = (channel_mode + 1) % 2;
            sound_stop_all();
            sound_init();
            tone_needs_trigger = true;
        }

        /* Manual Frequency tuning */
        if (!auto_sweep) {
            if (key_hit(KEY_LEFT)) cur_hz -= 10;
            else if (key_down(KEY_LEFT) && (tick & 3) == 0) cur_hz -= 10;

            if (key_hit(KEY_RIGHT)) cur_hz += 10;
            else if (key_down(KEY_RIGHT) && (tick & 3) == 0) cur_hz += 10;

            if (key_hit(KEY_DOWN)) cur_hz -= 100;
            else if (key_down(KEY_DOWN) && (tick & 7) == 0) cur_hz -= 100;

            if (key_hit(KEY_UP)) cur_hz += 100;
            else if (key_down(KEY_UP) && (tick & 7) == 0) cur_hz += 100;

            if (cur_hz < 50) cur_hz = 50;
            if (cur_hz > 10000) cur_hz = 10000;
        } else {
            /* Auto-sweep stepping */
            int sweep_step = 2;
            if (cur_hz >= 2000) sweep_step = 25;
            else if (cur_hz >= 500) sweep_step = 8;

            cur_hz += sweep_step;
            if (cur_hz > 10000) cur_hz = 50;
        }

        /* Smooth glitch-free tone update (only trigger on entry/channel switch, no 60Hz click!) */
        if (tone_needs_trigger) {
            tone_needs_trigger = false;
            if (channel_mode == 0) {
                u16 n = sweep_hz_to_psg(cur_hz);
                sound_set_pan(2, PAN_CENTER);
                REG_SOUND2CNT_L = (u16)((2 << 6) | (15 << 12)); /* 50% duty, max vol, sustained */
                REG_SOUND2CNT_H = (u16)(0x8000 | (n & 0x07FF));
            } else {
                u16 n = sweep_hz_to_wave(cur_hz);
                sound_play_wave(n, 0, PAN_CENTER);
            }
        } else if (cur_hz != prev_hz) {
            if (channel_mode == 0) {
                u16 n = sweep_hz_to_psg(cur_hz);
                REG_SOUND2CNT_H = (u16)(n & 0x07FF);
            } else {
                u16 n = sweep_hz_to_wave(cur_hz);
                REG_SOUND3CNT_X = (u16)(n & 0x07FF);
            }
        }

        /* Redraw screen frames */
        if (g_lang != prev_lang) {
            prev_lang = g_lang;
            need_redraw = true;
        }

        if (need_redraw) {
            gfx_clear(COLOR_NAVY);
            if (g_lang == LANG_DE) {
                gfx_draw_header("RESONANZ & FREQUENZ-SWEEP", "AUDIO");
                gfx_draw_footer("A:Sweep/Pause | L/R:Kanal | B:Ende");
                gfx_draw_card(4, 16, 232, 54, "GENERATOR & STEUERUNG", COLOR_CARD_BORDER);
                gfx_draw_card(4, 74, 232, 72, "OSZILLOSKOP & FEHLERSUCHE", COLOR_CARD_BORDER);
                gfx_draw_text(8, 128, "PFEILE: +/-10/100Hz | A: Auto", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
            } else {
                gfx_draw_header("RESONANCE & AUDIO SWEEP", "AUDIO");
                gfx_draw_footer("A:Sweep/Pause | L/R:Channel | B:Back");
                gfx_draw_card(4, 16, 232, 54, "GENERATOR & CONTROLS", COLOR_CARD_BORDER);
                gfx_draw_card(4, 74, 232, 72, "OSCILLOSCOPE & DIAGNOSIS", COLOR_CARD_BORDER);
                gfx_draw_text(8, 128, "D-PAD: +/-10/100Hz | A: Auto", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
            }
            need_redraw = false;
            prev_hz = -1;
            prev_channel_mode = -1;
            prev_auto_sweep = !auto_sweep;
            prev_hint_band = -1;
        }

        /* Dirty updates in Card 1 (zero flicker!) */
        if (cur_hz != prev_hz) {
            if (g_lang == LANG_DE) {
                gfx_draw_text_fmt(8, 29, COLOR_GOLD, COLOR_CARD_BG, false,
                                  "Frequenz : %5d Hz", cur_hz);
            } else {
                gfx_draw_text_fmt(8, 29, COLOR_GOLD, COLOR_CARD_BG, false,
                                  "Frequency: %5d Hz", cur_hz);
            }
        }
        if (channel_mode != prev_channel_mode) {
            prev_channel_mode = channel_mode;
            const char* ch_name = (channel_mode == 0) ?
                (g_lang == LANG_DE ? "Rechteck 2 (Harmonisch)" : "Square 2 (Harmonic)") :
                (g_lang == LANG_DE ? "Wave-RAM (Reiner Sinus)" : "Wave RAM (Pure Sine)");
            if (g_lang == LANG_DE) {
                gfx_draw_text_fmt(8, 41, COLOR_WHITE, COLOR_CARD_BG, false,
                                  "Kanal    : %-24s", ch_name);
            } else {
                gfx_draw_text_fmt(8, 41, COLOR_WHITE, COLOR_CARD_BG, false,
                                  "Channel  : %-24s", ch_name);
            }
        }
        if (auto_sweep != prev_auto_sweep) {
            prev_auto_sweep = auto_sweep;
            const char* st_name = auto_sweep ?
                (g_lang == LANG_DE ? "AUTO-SWEEP AKTIV" : "AUTO-SWEEP ACTIVE") :
                (g_lang == LANG_DE ? "MANUELL GEHALTEN" : "MANUAL HOLD");
            gfx_draw_text_fmt(8, 53, auto_sweep ? COLOR_EMERALD : COLOR_CYAN, COLOR_CARD_BG, false,
                              "Status   : %-20s", st_name);
        }
        prev_hz = cur_hz;

        /* Live waveform in Card 2 (only clears waveform strip y=86..107!) */
        gfx_draw_rect(8, 86, 224, 22, COLOR_CARD_BG);

        /* Draw animated visual wave */
        int wave_y_center = 97;
        int phase = (tick * 4) & 31;
        for (int x = 12; x < 228; x += 2) {
            int wave_h = 0;
            if (channel_mode == 0) {
                int p = (x * cur_hz / 100 + phase) % 20;
                wave_h = (p < 10) ? -5 : 5;
            } else {
                int p = (x * cur_hz / 100 + phase) % 16;
                static const int lut[16] = { 0, 2, 4, 6, 7, 6, 4, 2, 0, -2, -4, -6, -7, -6, -4, -2 };
                wave_h = lut[p];
            }
            gfx_draw_pixel(x, wave_y_center + wave_h, COLOR_CYAN_GLOW);
            gfx_draw_pixel(x + 1, wave_y_center + wave_h, COLOR_CYAN);
        }

        /* Diagnostic hint updated ONLY when frequency band changes */
        int hint_band = (cur_hz < 300) ? 0 : ((cur_hz < 1500) ? 1 : 2);
        if (hint_band != prev_hint_band) {
            prev_hint_band = hint_band;
            gfx_draw_rect(8, 114, 224, 10, COLOR_CARD_BG);
            if (g_lang == LANG_DE) {
                const char* hint = (hint_band == 0) ? "50-250Hz: Gehaeuse & Akkudeckel"
                                 : (hint_band == 1) ? "300-1kHz: Lautsprecher-Membran"
                                                    : "1.5-4kHz: Displayglas & Tasten";
                gfx_draw_text(8, 114, hint, COLOR_EMERALD, COLOR_CARD_BG, false);
            } else {
                const char* hint = (hint_band == 0) ? "50-250Hz: Casing & Battery Door"
                                 : (hint_band == 1) ? "300-1kHz: Speaker Cone Rattle"
                                                    : "1.5-4kHz: Lens & Button Seats";
                gfx_draw_text(8, 114, hint, COLOR_EMERALD, COLOR_CARD_BG, false);
            }
        }
    }

    sound_stop_all();
}

static void draw_audio_row(int i, bool active) {
    int y = 28 + i * 11;
    u16 bg_col = active ? COLOR_CARD_BORDER : COLOR_CARD_BG;
    u16 fg_col = active ? COLOR_WHITE : COLOR_LIGHT_GRAY;

    gfx_draw_rect(10, y - 1, 220, 10, bg_col);
    if (active) {
        gfx_draw_text(14, y + 1, ">", COLOR_CYAN, bg_col, false);
    }
    gfx_draw_text(22, y + 1, items[g_lang][i], fg_col, bg_col, false);
}

static void redraw_audio_menu(int sel) {
    gfx_clear(COLOR_NAVY);
    if (g_lang == LANG_DE) {
        gfx_draw_header("SOUND & AUDIO-SUITE", "B: ENDE");
        gfx_draw_footer("AUF/AB:Wahl | A:Play | SEL:Info | B");
        gfx_draw_card(6, 17, 228, 127, "AUDIO-HARDWARE & SEQUENCER", COLOR_CARD_BORDER);
    } else {
        gfx_draw_header("SOUND & AUDIO SUITE", "B: EXIT");
        gfx_draw_footer("UP/DN:Nav | A:Play | SEL:Guide | B");
        gfx_draw_card(6, 17, 228, 127, "AUDIO HARDWARE & SEQUENCER", COLOR_CARD_BORDER);
    }

    for (int i = 0; i < ITEM_COUNT; i++) {
        draw_audio_row(i, i == sel);
    }
}

void run_audio_test(void) {
    int sel = 0;
    int prev_sel = -1;
    Language prev_lang = g_lang;

    redraw_audio_menu(sel);
    prev_sel = sel;

    while (1) {
        vsync();
        input_poll();

        if (key_hit(KEY_B)) {
            sound_stop_all();
            break;
        }

        if (key_hit(KEY_UP)) {
            sel = (sel - 1 + ITEM_COUNT) % ITEM_COUNT;
        }
        if (key_hit(KEY_DOWN)) {
            sel = (sel + 1) % ITEM_COUNT;
        }

        if (g_lang != prev_lang) {
            prev_lang = g_lang;
            redraw_audio_menu(sel);
            prev_sel = sel;
        }

        /* 2. Dirty row update */
        if (sel != prev_sel) {
            draw_audio_row(prev_sel, false);
            draw_audio_row(sel, true);
            prev_sel = sel;
        }

        /* 3. Action when A is pressed */
        if (key_hit(KEY_A)) {
            if (sel == 7) {
                /* Launch Speaker Resonance & Audio Sweep Screen */
                sound_stop_all();
                run_audio_sweep_screen();
                redraw_audio_menu(sel);
                continue;
            }

#ifndef STANDALONE_MULTIBOOT
            if (sel == 8) {
                /* Launch MIDI Sequencer */
                sound_stop_all();
                run_midi_player();
                redraw_audio_menu(sel);
                continue;
            }
#endif

            /* Reset audio hardware cleanly before every test */
            sound_stop_all();
            sound_init();

            switch (sel) {
                case 0:
                    gfx_draw_rect(10, 126, 220, 15, COLOR_CARD_BG);
                    if (g_lang == LANG_DE) {
                        gfx_draw_text(10, 128, "CH1: RECHTECK 1 SWEEP...", COLOR_AMBER, COLOR_CARD_BG, false);
                    } else {
                        gfx_draw_text(10, 128, "CH1: SQUARE 1 PITCH SWEEP...", COLOR_AMBER, COLOR_CARD_BG, false);
                    }
                    sound_play_square1(1750, 2, 2, true, PAN_CENTER);
                    for (int f = 0; f < 45; f++) {
                        vsync(); input_poll();
                        if (key_hit(KEY_B)) break;
                    }
                    break;

                case 1:
                    gfx_draw_rect(10, 126, 220, 15, COLOR_CARD_BG);
                    if (g_lang == LANG_DE) {
                        gfx_draw_text(10, 128, "CH2: RECHTECK 2 LEAD (440Hz)...", COLOR_CYAN, COLOR_CARD_BG, false);
                    } else {
                        gfx_draw_text(10, 128, "CH2: SQUARE 2 LEAD (440Hz)...", COLOR_CYAN, COLOR_CARD_BG, false);
                    }
                    sound_play_square2(1750, 2, PAN_CENTER);
                    for (int f = 0; f < 45; f++) {
                        vsync(); input_poll();
                        if (key_hit(KEY_B)) break;
                    }
                    break;

                case 2:
                    gfx_draw_rect(10, 126, 220, 15, COLOR_CARD_BG);
                    if (g_lang == LANG_DE) {
                        gfx_draw_text(10, 128, "CH3: WAVE-RAM SYNTH (440Hz)...", COLOR_EMERALD, COLOR_CARD_BG, false);
                    } else {
                        gfx_draw_text(10, 128, "CH3: WAVE RAM SYNTH (440Hz)...", COLOR_EMERALD, COLOR_CARD_BG, false);
                    }
                    sound_play_wave(1899, 0, PAN_CENTER);
                    for (int f = 0; f < 45; f++) {
                        vsync(); input_poll();
                        if (key_hit(KEY_B)) break;
                    }
                    break;

                case 3:
                    gfx_draw_rect(10, 126, 220, 15, COLOR_CARD_BG);
                    if (g_lang == LANG_DE) {
                        gfx_draw_text(10, 128, "CH4: RAUSCH-GENERATOR (DRUMS)...", COLOR_CORAL, COLOR_CARD_BG, false);
                    } else {
                        gfx_draw_text(10, 128, "CH4: NOISE DRUM PERCUSSION...", COLOR_CORAL, COLOR_CARD_BG, false);
                    }
                    sound_play_noise(2, false, PAN_CENTER);
                    for (int f = 0; f < 45; f++) {
                        vsync(); input_poll();
                        if (key_hit(KEY_B)) break;
                    }
                    break;

                case 4:
                    gfx_draw_rect(10, 126, 220, 15, COLOR_CARD_BG);
                    gfx_draw_text(10, 128, "DIRECTSOUND 8-BIT PCM (16kHz)...", COLOR_GOLD, COLOR_CARD_BG, false);
                    sound_play_directsound_a(directsound_pcm, DIRECTSOUND_SAMPLE_COUNT, PAN_CENTER, 100);
                    for (int f = 0; f < 92; f++) {
                        vsync(); input_poll();
                        if (key_hit(KEY_B)) break;
                    }
                    sound_stop_directsound();
                    break;

                case 5: {
                    bool aborted = false;
                    gfx_draw_rect(10, 126, 220, 15, COLOR_CARD_BG);
                    gfx_draw_text(10, 128, (g_lang == LANG_DE) ? "LINKS..." : "LEFT SPEAKER...",
                                  COLOR_AMBER, COLOR_CARD_BG, false);
                    sound_play_square2(1850, 1, PAN_LEFT);
                    for (int f = 0; f < 35; f++) {
                        vsync(); input_poll();
                        if (key_hit(KEY_B)) { aborted = true; break; }
                    }
                    if (aborted) break;

                    gfx_draw_rect(10, 126, 220, 15, COLOR_CARD_BG);
                    gfx_draw_text(10, 128, (g_lang == LANG_DE) ? "RECHTS..." : "RIGHT SPEAKER...",
                                  COLOR_CYAN, COLOR_CARD_BG, false);
                    sound_play_square2(1650, 1, PAN_RIGHT);
                    for (int f = 0; f < 35; f++) {
                        vsync(); input_poll();
                        if (key_hit(KEY_B)) { aborted = true; break; }
                    }
                    if (aborted) break;

                    gfx_draw_rect(10, 126, 220, 15, COLOR_CARD_BG);
                    gfx_draw_text(10, 128, (g_lang == LANG_DE) ? "STEREO MITTE..." : "STEREO CENTER...",
                                  COLOR_EMERALD, COLOR_CARD_BG, false);
                    sound_play_square2(1750, 2, PAN_CENTER);
                    for (int f = 0; f < 35; f++) {
                        vsync(); input_poll();
                        if (key_hit(KEY_B)) break;
                    }
                    break;
                }

                case 6: {
                    bool aborted = false;
                    gfx_draw_rect(10, 126, 220, 15, COLOR_CARD_BG);
                    if (g_lang == LANG_DE) {
                        gfx_draw_text(10, 128, "CHROMATISCHE SKALA (13 TOENE)...", COLOR_GOLD, COLOR_CARD_BG, false);
                    } else {
                        gfx_draw_text(10, 128, "CHROMATIC SCALE (13 NOTES)...", COLOR_GOLD, COLOR_CARD_BG, false);
                    }
                    static const u16 notes[] = { 1546, 1575, 1602, 1628, 1652, 1675, 1697, 1717, 1737, 1755, 1772, 1788, 1803 };
                    for (int n = 0; n < 13 && !aborted; n++) {
                        sound_play_square2(notes[n], 2, (n % 2 == 0) ? PAN_LEFT : PAN_RIGHT);
                        for (int f = 0; f < 6; f++) {
                            vsync(); input_poll();
                            if (key_hit(KEY_B)) { aborted = true; break; }
                        }
                    }
                    break;
                }
            }

            /* Clean up banner and stop sound after playback */
            gfx_draw_rect(10, 126, 220, 15, COLOR_CARD_BG);
            sound_stop_all();
        }

        /* 4. Show comprehensive audio documentation on SELECT */
        if (key_hit(KEY_SELECT)) {
            sound_stop_all();
            run_help_guide(HELP_TOPIC_AUDIO);
            redraw_audio_menu(sel);
            prev_sel = sel;
        }
    }
}
