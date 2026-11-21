#include "../include/midi_player.h"
#include "../include/midi_data.h"
#include "../include/sound.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/directsound_data.h"
#include "../include/directsound_drums.h"
#include "../include/directsound_instruments.h"

/* GBA Standard Save Identification string so EZ-Flash Omega enables SRAM patch */
static const char g_sram_ident[] __attribute__((used)) = "SRAM_V113";

#define MIDI_SAVE_MAGIC 0x4D494449 /* "MIDI" */

#define NUM_TONES_CH1 12
#define NUM_TONES_CH2 12
#define NUM_TONES_CH3 12
#define NUM_TONES_CH4 12
#define NUM_TONES_DS  24

static int g_modal_depth = 0;

static inline int get_num_tones(int ch) {
    if (ch == 0) return NUM_TONES_CH1;
    if (ch == 1) return NUM_TONES_CH2;
    if (ch == 2) return NUM_TONES_CH3;
    if (ch == 3) return NUM_TONES_CH4;
    return NUM_TONES_DS;
}

/* Channel 1: Pulse 1 with Hardware Sweep (12 distinct sound options) */
static const char* ch1_tones[12] = {
    "50% Warm Analog Lead",
    "12.5% Sharp FM Synth",
    "25% Classic NES Pulse",
    "75% Mellow Flute Pulse",
    "Fast Laser Arp (Up Sweep)",
    "Divebomb Laser (Dn Sweep)",
    "Dynamic Brass Horn (Punch)",
    "Pizzicato Staccato Harp",
    "Nostalgic Music Box (Bell)",
    "Deep Moog Pulse Bass",
    "Acid Zap Lead (Fast Sweep)",
    "Sustained Cathedral Organ"
};
static const u8 ch1_duty[12]  = { 2, 0, 1, 3, 2, 2, 3, 1, 0, 2, 0, 2 };
static const u8 ch1_env[12]   = { 3, 2, 2, 0, 3, 2, 2, 1, 1, 2, 3, 0 };
static const u8 ch1_sweep[12] = { 0, 0, 0, 0, 0x12, 0x1A, 0, 0, 0, 0x09, 0x22, 0 };

/* Channel 2: Pulse 2 Melodic Voice (12 distinct sound options) */
static const char* ch2_tones[12] = {
    "Dreamy Rhodes E-Piano",
    "Warm Flute / Woodwind",
    "Melodic String Quartet",
    "Celesta / Starlight Bell",
    "Fat 8-Bit Retro Chiptune",
    "Soft Acoustic Nylon Pluck",
    "Slap Octave Pulse Bass",
    "Tremolo Shimmer Lead",
    "Bright Accordion / Reed",
    "Sub-Heavy Square Bass",
    "Harpsichord Crisp Pluck",
    "Pure Sustained Sine Pad"
};
static const u8 ch2_duty[12] = { 1, 2, 2, 0, 3, 1, 0, 2, 0, 2, 1, 2 };
static const u8 ch2_env[12]  = { 2, 0, 0, 1, 3, 1, 2, 4, 0, 3, 1, 0 };

/* Channel 3: 32-Sample Wave RAM Synthesizer (12 distinct waveforms) */
static const char* ch3_tones[12] = {
    "Melancholy Dark Piano (Wave)",
    "Deep 808 Sub / Sine Bass",
    "Acoustic Cello / Bowed String",
    "Rhodes Electric Piano Wave",
    "Vocal 'Ahhh' Formant Choir",
    "DMG Classic Triangle Bass",
    "Harpsichord / Acoustic Harp",
    "Retro Acid Synth (Buzz)",
    "Glockenspiel / Glass Bell",
    "Accordion / Reed Organ",
    "Slap Thumb Bass Wave",
    "Oboe / Reedy Pipe Wave"
};
static const u16 wave_presets[12][8] = {
    { 0x8976, 0xAB87, 0xDECA, 0xEFFE, 0xDCBA, 0x9876, 0x5432, 0x3456 }, /* 0: Melancholy Dark Piano */
    { 0xDF8B, 0xECFF, 0x64A8, 0x0021, 0x2401, 0xAC68, 0xFFEF, 0x98DB }, /* 1: Deep 808 Sine Sub */
    { 0xBD97, 0xFEDC, 0x7531, 0x1357, 0xBD97, 0xFEDC, 0x7531, 0x1357 }, /* 2: Sad Cello / Strings */
    { 0x6745, 0x2301, 0xEFCD, 0xAB89, 0x98BA, 0xDCFE, 0x1032, 0x5476 }, /* 3: E-Piano / Rhodes */
    { 0x8AB9, 0xCDDC, 0xBA98, 0x6545, 0x6789, 0xBCDE, 0xFEDC, 0xBA98 }, /* 4: Vocal Formant Choir */
    { 0x0123, 0x4567, 0x89AB, 0xCDEF, 0xFEDC, 0xBA98, 0x7654, 0x3210 }, /* 5: DMG Classic Triangle Bass */
    { 0xDFBA, 0xEC64, 0x8201, 0x3579, 0xACDF, 0xDB97, 0x5310, 0x2468 }, /* 6: Harpsichord / Acoustic Harp */
    { 0xFFFF, 0xEEEE, 0xDDDD, 0xCCCC, 0x8844, 0x2211, 0x0000, 0x0000 }, /* 7: Retro Acid Synth Buzz */
    { 0xFD97, 0x5310, 0x0000, 0x0135, 0x79BD, 0xFEDC, 0xBA98, 0x7654 }, /* 8: Glockenspiel / Glass Bell */
    { 0x7E3D, 0x5C2B, 0x8F4E, 0x6D1A, 0x7E3D, 0x5C2B, 0x8F4E, 0x6D1A }, /* 9: Accordion / Reed Organ */
    { 0x0258, 0xBDFA, 0x8410, 0x0012, 0x479B, 0xDFDB, 0x9642, 0x1000 }, /* 10: Slap Thumb Bass Wave */
    { 0x468A, 0xCEF9, 0x5200, 0x0014, 0x79BD, 0xFACE, 0x9631, 0x0000 }  /* 11: Oboe / Reedy Pipe Wave */
};

/* Channel 4: PSG Noise Engine (12 distinct percussion & pitched synth modes) */
static const char* ch4_tones[12] = {
    "Punchy 808/909 Electronic Drum",  /* 0: Heavy sub kick, snappy crack snare, razor hats */
    "Acoustic Studio Drum Kit",        /* 1: Dynamic wood snare, round kick, natural cymbals */
    "8-Bit Retro Chiptune Kit",        /* 2: Classic Game Boy crunchy DMG noise percussion */
    "Heavy Rock & Metal Kit",          /* 3: Massive explosive snare decay, booming toms & crash */
    "Pitched 7-Bit Robotic Bass",      /* 4: TUNED PITCH: Gritty chiptune robotic synth bass */
    "Pitched Metallic Cyber Lead",     /* 5: TUNED PITCH: Industrial metallic tonal lead synth */
    "Hi-Tech Cyber Glitch & Laser",    /* 6: Futuristic pitch zaps, laser clicks, modulated SFX */
    "Soft Lo-Fi Brushes & Shakers",    /* 7: Smooth gentle acoustic shakers and jazz brushes */
    "Industrial Hardcore Kick & Snare",/* 8: Distorted aggressive punch percussion */
    "Laser Tom & Retro Zaps",          /* 9: Sci-fi frequency sweeps & toms */
    "Dubstep Metallic Snare & Hat",    /* 10: Crisp high-frequency punch snare */
    "Arcade Explosion & Sci-Fi SFX"    /* 11: Long decaying noise burst & retro boom */
};

/* DirectSound: 24 Distinct, High-Quality PCM Instruments (Shared between DSA & DSB!) */
static const char* ds_tones[24] = {
    "Real PCM Drum Kit (808/Studio)",
    "Concert Grand Piano (Acoustic)",
    "Expressive Solo Violin (Warm)",
    "Warm Bowed Cello (Acoustic)",
    "Orchestral Strings Ensemble",
    "Acoustic Woodwind Flute",
    "Brass Horn Fanfare (Trumpet)",
    "Overdrive Rock Guitar (Lead)",
    "Acoustic Nylon Guitar (Pluck)",
    "Vintage Rhodes E-Piano (Tine)",
    "Funk Slap Bass (Punchy Pop)",
    "Analog Saw Lead Synth (TB-303)",
    "Deep 808 Sub-Bass (Punchy)",
    "Pure Analog Sine Lead (Warm)",
    "Baroque Harpsichord (Pluck)",
    "Cathedral Pipe Organ (Grand)",
    "Bright Pop Piano (Upright/Honky)",
    "Orchestral Pizzicato Strings",
    "Warm Reedy Oboe (Double Reed)",
    "Muted French Horn (Warm Brass)",
    "Clean Jazz Guitar (Archtop)",
    "Crystal FM E-Piano (DX7 Tine)",
    "Acoustic Upright Jazz Bass",
    "Warm Analog Pad (Juno Synth)"
};

static const s8* const ds_melodic_samples[24] = {
    0,
    ds_piano_pcm,
    ds_violin_pcm,
    ds_cello_pcm,
    ds_strings_pcm,
    ds_flute_pcm,
    ds_brass_pcm,
    ds_guitar_pcm,
    ds_nylon_pcm,
    ds_rhodes_pcm,
    ds_bass_pcm,
    ds_synth_pcm,
    ds_subbass_pcm,
    ds_sine_pcm,
    ds_harpsichord_pcm,
    ds_organ_pcm,
    ds_piano2_pcm,
    ds_pizz_pcm,
    ds_oboe_pcm,
    ds_frenchhorn_pcm,
    ds_jazzgtr_pcm,
    ds_dx7_pcm,
    ds_upright_pcm,
    ds_pad_pcm
};

static const int ds_melodic_lengths[24] = {
    0,
    sizeof(ds_piano_pcm),
    sizeof(ds_violin_pcm),
    sizeof(ds_cello_pcm),
    sizeof(ds_strings_pcm),
    sizeof(ds_flute_pcm),
    sizeof(ds_brass_pcm),
    sizeof(ds_guitar_pcm),
    sizeof(ds_nylon_pcm),
    sizeof(ds_rhodes_pcm),
    sizeof(ds_bass_pcm),
    sizeof(ds_synth_pcm),
    sizeof(ds_subbass_pcm),
    sizeof(ds_sine_pcm),
    sizeof(ds_harpsichord_pcm),
    sizeof(ds_organ_pcm),
    sizeof(ds_piano2_pcm),
    sizeof(ds_pizz_pcm),
    sizeof(ds_oboe_pcm),
    sizeof(ds_frenchhorn_pcm),
    sizeof(ds_jazzgtr_pcm),
    sizeof(ds_dx7_pcm),
    sizeof(ds_upright_pcm),
    sizeof(ds_pad_pcm)
};

/* Channel Identity Colors matching the Waterfall and Piano */
static const u16 ch_colors[6] = {
    COLOR_CYAN,      /* CH1: Cyan */
    COLOR_GOLD,      /* CH2: Gold */
    COLOR_EMERALD,   /* CH3: Emerald */
    0x599F,          /* CH4: Purple / Plum PSG Noise */
    COLOR_CORAL,     /* DSA: Coral / Orange */
    0x7E10           /* DSB: Sky Blue / Ice */
};
static const char* ch_names[6] = { "CH1", "CH2", "CH3", "CH4", "DSA", "DSB" };

static inline u8 normalize_vol(u8 v) {
    if (v <= 3) return (v + 1) * 25; /* Migrate legacy 0..3 to 25%, 50%, 75%, 100% */
    if (v > 100) return 100;
    return (v / 5) * 5; /* Snap to 5% steps */
}

/* Panning options (1=Left, 2=Right, 3=Center) */
static const char* pan_names[3] = { "LEFT  ", "RIGHT ", "CENTER" };
static const u8 pan_values[3] = { PAN_LEFT, PAN_RIGHT, PAN_CENTER };

/* State definitions for the 6-channel active sequencer */
static int  g_cur_song_idx = 0;
static u16  g_ch_track_mask[6] = { (1 << 0), (1 << 1), (1 << 2), (1 << 3), (1 << 4), (1 << 5) };
static u8   g_ch_tone[6] = { 0, 0, 0, 0, 0, 1 }; /* Default DSA=Drums (0), DSB=Piano (1) */
static bool g_ch_mute[6] = { false, false, false, false, false, false };
static bool g_ch_solo[6] = { false, false, false, false, false, false };
static s8   g_ch_octave[6] = { 0, 0, 0, 0, 0, 0 };
static u8   g_ch_vol[6] = { 100, 100, 100, 100, 100, 100 }; /* 100% default in 5% steps */
static u8   g_ch_pan[6] = { 2, 2, 2, 2, 2, 2 }; /* Center default */

static inline bool has_any_solo(void) {
    for (int i = 0; i < 6; i++) {
        if (g_ch_solo[i]) return true;
    }
    return false;
}

static inline bool is_channel_audible(int c) {
    if (g_ch_mute[c]) return false;
    if (g_ch_vol[c] == 0) return false;
    if (has_any_solo()) {
        return g_ch_solo[c];
    }
    return true;
}

static bool g_is_playing = true;
static u32  g_cur_frame = 0;
static int  g_event_idx = 0;
static u8   g_ch_cur_note[6] = { 0, 0, 0, 0, 0, 0 };
static u8   g_ch_vis_note[6] = { 0, 0, 0, 0, 0, 0 };
static u8   g_ch_vis_hold[6] = { 0, 0, 0, 0, 0, 0 };
static int  g_ds_frames_left[2] = { 0, 0 };

static void assign_default_tracks_for_song(int song_idx) {
    const SongInfo* song = &g_midi_songs[song_idx];
    int ntrks = song->track_count;

    int drum_trk = -1;
    for (int t = 0; t < ntrks; t++) {
        const char* nm = song->tracks[t].name;
        if (nm) {
            for (const char* p = nm; *p; p++) {
                if ((p[0] == 'D' || p[0] == 'd') && (p[1] == 'r' || p[1] == 'R')) { drum_trk = t; break; }
                if ((p[0] == 'P' || p[0] == 'p') && (p[1] == 'e' || p[1] == 'E')) { drum_trk = t; break; }
            }
        }
        if (drum_trk >= 0) break;
    }
    if (drum_trk < 0 && ntrks >= 4) drum_trk = 3;

    if (ntrks <= 1) {
        g_ch_track_mask[0] = (1 << 0);
        g_ch_track_mask[1] = 0;
        g_ch_track_mask[2] = 0;
        g_ch_track_mask[3] = 0;
        g_ch_track_mask[4] = 0;
        g_ch_track_mask[5] = (1 << 0); /* DSB doubles melody with Concert Grand Piano */
    } else if (ntrks <= 3) {
        g_ch_track_mask[0] = (1 << 0);
        g_ch_track_mask[1] = (1 << 1);
        g_ch_track_mask[2] = (ntrks > 2) ? (1 << 2) : (1 << 0);
        g_ch_track_mask[3] = 0;
        g_ch_track_mask[4] = 0;
        g_ch_track_mask[5] = (1 << 0); /* DSB Concert Grand Piano */
    } else if (ntrks == 4) {
        g_ch_track_mask[0] = (1 << 0); /* Lead */
        g_ch_track_mask[1] = (1 << 1); /* Harmony */
        g_ch_track_mask[2] = (1 << 2); /* Bass */
        g_ch_track_mask[3] = (1 << 3); /* Noise Drums */
        g_ch_track_mask[4] = (1 << 3); /* DSA: Real PCM Drum Kit */
        g_ch_track_mask[5] = (1 << 0); /* DSB: Concert Grand Piano doubling lead */
    } else {
        g_ch_track_mask[0] = (1 << 0);
        g_ch_track_mask[1] = (1 << 1);
        g_ch_track_mask[2] = (1 << 2);
        g_ch_track_mask[3] = (drum_trk >= 0) ? (1 << drum_trk) : (1 << 3);
        g_ch_track_mask[4] = (drum_trk >= 0) ? (1 << drum_trk) : (1 << 4);
        g_ch_track_mask[5] = (ntrks > 5) ? (1 << 5) : ((ntrks > 4) ? (1 << 4) : (1 << 0));
    }
}

static void apply_wave_ram(int wave_idx) {
    const u16* wave = wave_presets[wave_idx % NUM_TONES_CH3];
    REG_SOUND3CNT_L = 0x0000;
    REG_SOUND3CNT_X = 0x0000;

    vu16* wave_ram = (vu16*)(MEM_IO + 0x0090);
    REG_SOUND3CNT_L = 0x0040;
    for (int i = 0; i < 8; i++) wave_ram[i] = wave[i];
    REG_SOUND3CNT_L = 0x0000;
    for (int i = 0; i < 8; i++) wave_ram[i] = wave[i];

    REG_SOUND3CNT_L = 0x0080;
    u16 w_vol;
    if (g_ch_vol[2] == 0) w_vol = 0x0000;
    else if (g_ch_vol[2] <= 35) w_vol = 0x6000; /* 25% */
    else if (g_ch_vol[2] <= 65) w_vol = 0x4000; /* 50% */
    else if (g_ch_vol[2] <= 85) w_vol = 0x8000; /* 75% */
    else w_vol = 0x2000; /* 100% */
    REG_SOUND3CNT_H = w_vol;
}

static void silence_hardware(void) {
    REG_SOUND1CNT_H = 0;
    REG_SOUND1CNT_X = 0x8000;
    REG_SOUND2CNT_L = 0;
    REG_SOUND2CNT_H = 0x8000;
    REG_SOUND3CNT_H = 0;
    REG_SOUND3CNT_L = 0;
    REG_SOUND4CNT_L = 0;
    REG_SOUND4CNT_H = 0x8000;
    sound_stop_directsound();
    g_ds_frames_left[0] = 0;
    g_ds_frames_left[1] = 0;
}

static void silence_channel(int c) {
    if (c == 0) {
        REG_SOUND1CNT_H = 0;
        REG_SOUND1CNT_X = 0x8000;
    } else if (c == 1) {
        REG_SOUND2CNT_L = 0;
        REG_SOUND2CNT_H = 0x8000;
    } else if (c == 2) {
        REG_SOUND3CNT_H = 0x0000;
        REG_SOUND3CNT_L = 0x0000;
    } else if (c == 3) {
        REG_SOUND4CNT_L = 0;
        REG_SOUND4CNT_H = 0x8000;
    } else if (c == 4) {
        sound_stop_directsound_a();
        g_ds_frames_left[0] = 0;
    } else if (c == 5) {
        sound_stop_directsound_b();
        g_ds_frames_left[1] = 0;
    }
}

static void apply_channel_volume(int ch) {
    if (ch == 0) {
        if (g_ch_vol[0] == 0 || !is_channel_audible(0)) {
            silence_channel(0);
        } else if (g_ch_cur_note[0] > 0) {
            u8 vol = (g_ch_vol[0] * 15 + 50) / 100;
            int tone = g_ch_tone[0] % NUM_TONES_CH1;
            u8 duty = ch1_duty[tone];
            u8 env = ch1_env[tone];
            REG_SOUND1CNT_H = (u16)(((duty & 3) << 6) | (vol << 12) | (env << 8));
        }
    } else if (ch == 1) {
        if (g_ch_vol[1] == 0 || !is_channel_audible(1)) {
            silence_channel(1);
        } else if (g_ch_cur_note[1] > 0) {
            u8 vol = (g_ch_vol[1] * 15 + 50) / 100;
            int tone = g_ch_tone[1] % NUM_TONES_CH2;
            u8 duty = ch2_duty[tone];
            u8 env = ch2_env[tone];
            REG_SOUND2CNT_L = (u16)(((duty & 3) << 6) | (vol << 12) | (env << 8));
        }
    } else if (ch == 2) {
        apply_wave_ram(g_ch_tone[2]);
    } else if (ch == 3) {
        if (g_ch_vol[3] == 0 || !is_channel_audible(3)) {
            silence_channel(3);
        }
    } else if (ch == 4) {
        if (g_ch_vol[4] == 0 || !is_channel_audible(4)) {
            sound_stop_directsound_a();
            g_ds_frames_left[0] = 0;
        } else {
            u16 h = REG_SOUNDCNT_H & ~(1 << 2);
            if (g_ch_vol[4] >= 50) h |= (1 << 2);
            REG_SOUNDCNT_H = h;
        }
    } else if (ch == 5) {
        if (g_ch_vol[5] == 0 || !is_channel_audible(5)) {
            sound_stop_directsound_b();
            g_ds_frames_left[1] = 0;
        } else {
            u16 h = REG_SOUNDCNT_H & ~(1 << 3);
            if (g_ch_vol[5] >= 50) h |= (1 << 3);
            REG_SOUNDCNT_H = h;
        }
    }
}

/* --- Non-volatile Cartridge SRAM / Multi-Preset Storage --- */
#define MIDI_PRESETS_PER_SONG 3
#define MIDI_SAVE_MAGIC_V6 0x3644494D /* "MID6" */

typedef struct {
    u16 track_mask[6];
    u8  tone[6];
    u8  mute[6];
    s8  octave[6];
    u8  vol[6];
    u8  pan[6];
    u8  is_valid;
    u8  reserved;
} SongPreset;

static SongPreset g_song_presets[MIDI_SONG_COUNT][MIDI_PRESETS_PER_SONG];

static void midi_save_config(void) {
    vu8* sram = (vu8*)MEM_SRAM;
    sram[0] = 'M';
    sram[1] = 'I';
    sram[2] = 'D';
    sram[3] = '6';
    sram[4] = 6; /* Version 6 (Multi-preset per song) */
    sram[5] = (u8)(g_cur_song_idx % MIDI_SONG_COUNT);
    sram[6] = MIDI_SONG_COUNT;
    sram[7] = MIDI_PRESETS_PER_SONG;

    const u8* src = (const u8*)g_song_presets;
    int data_len = (int)sizeof(g_song_presets);
    for (int i = 0; i < data_len; i++) {
        sram[8 + i] = src[i];
    }

    u8 xor_sum = 0;
    u8 add_sum = 0;
    for (int i = 0; i < 8 + data_len; i++) {
        xor_sum ^= sram[i];
        add_sum += sram[i];
    }
    sram[8 + data_len] = xor_sum;
    sram[8 + data_len + 1] = add_sum;
}

static bool midi_load_config(void) {
    vu8* sram = (vu8*)MEM_SRAM;
    u32 magic = ((u32)sram[0]) | ((u32)sram[1] << 8) | ((u32)sram[2] << 16) | ((u32)sram[3] << 24);

    /* 1. Check Version 6 Format ("MID6") */
    if (magic == MIDI_SAVE_MAGIC_V6 && sram[4] == 6 && sram[6] == MIDI_SONG_COUNT) {
        int data_len = (int)sizeof(g_song_presets);
        u8 xor_sum = 0;
        u8 add_sum = 0;
        for (int i = 0; i < 8 + data_len; i++) {
            xor_sum ^= sram[i];
            add_sum += sram[i];
        }
        if (xor_sum == sram[8 + data_len] && add_sum == sram[8 + data_len + 1]) {
            g_cur_song_idx = sram[5] % MIDI_SONG_COUNT;
            u8* dst = (u8*)g_song_presets;
            for (int i = 0; i < data_len; i++) {
                dst[i] = sram[8 + i];
            }
            return true;
        }
    }

    /* 2. Upgrade from legacy Version 5 Format ("MIDI") */
    if (magic == MIDI_SAVE_MAGIC && sram[4] == 5) {
        u8 sum = 0;
        for (int i = 0; i < 48; i++) sum ^= sram[i];
        if (sum == sram[48]) {
            int old_song = sram[5] % MIDI_SONG_COUNT;
            g_cur_song_idx = old_song;
            /* Clear all presets */
            u8* dst = (u8*)g_song_presets;
            for (int i = 0; i < (int)sizeof(g_song_presets); i++) dst[i] = 0;

            SongPreset* p = &g_song_presets[old_song][0];
            for (int c = 0; c < 6; c++) {
                p->track_mask[c] = (u16)sram[6 + c * 2] | ((u16)sram[7 + c * 2] << 8);
                p->tone[c] = sram[18 + c] & 7;
                p->mute[c] = (sram[24 + c] != 0);
                s8 oct = (s8)sram[30 + c];
                if (oct < -2) oct = -2;
                if (oct > 2) oct = 2;
                p->octave[c] = oct;
                p->vol[c] = sram[36 + c] & 3;
                p->pan[c] = sram[42 + c] % 3;
            }
            p->is_valid = 1;
            midi_save_config(); /* Upgrade to V6 */
            return true;
        }
    }

    /* Uninitialized / blank SRAM */
    u8* dst = (u8*)g_song_presets;
    for (int i = 0; i < (int)sizeof(g_song_presets); i++) dst[i] = 0;
    return false;
}

static void load_song_active_configuration(int song_idx) {
    if (song_idx < 0 || song_idx >= MIDI_SONG_COUNT) return;
    if (g_song_presets[song_idx][0].is_valid) {
        /* Apply custom preset Slot 1 immediately */
        const SongPreset* p = &g_song_presets[song_idx][0];
        for (int c = 0; c < 6; c++) {
            g_ch_track_mask[c] = p->track_mask[c];
            g_ch_tone[c] = p->tone[c] % get_num_tones(c);
            g_ch_mute[c] = (p->mute[c] != 0);
            g_ch_octave[c] = p->octave[c];
            g_ch_vol[c] = normalize_vol(p->vol[c]);
            g_ch_pan[c] = p->pan[c] % 3;
            sound_set_pan(c + 1, pan_values[g_ch_pan[c] % 3]);
            if (g_ch_mute[c]) silence_channel(c);
            else apply_channel_volume(c);
        }
    } else {
        /* No custom preset saved for this song -> assign defaults */
        assign_default_tracks_for_song(song_idx);
        for (int c = 0; c < 6; c++) {
            g_ch_tone[c] = (c == 5) ? 1 : 0;
            g_ch_mute[c] = false;
            g_ch_octave[c] = 0;
            g_ch_vol[c] = 100;
            g_ch_pan[c] = 2;
            sound_set_pan(c + 1, pan_values[2]);
            apply_channel_volume(c);
        }
    }
    apply_wave_ram(g_ch_tone[2]);
}

static void apply_preset(int song_idx, int slot_idx) {
    if (song_idx < 0 || song_idx >= MIDI_SONG_COUNT) return;
    if (slot_idx < 0 || slot_idx >= MIDI_PRESETS_PER_SONG) return;
    if (!g_song_presets[song_idx][slot_idx].is_valid) return;
    const SongPreset* p = &g_song_presets[song_idx][slot_idx];
    for (int c = 0; c < 6; c++) {
        g_ch_track_mask[c] = p->track_mask[c];
        g_ch_tone[c] = p->tone[c] % get_num_tones(c);
        g_ch_mute[c] = (p->mute[c] != 0);
        g_ch_octave[c] = p->octave[c];
        g_ch_vol[c] = normalize_vol(p->vol[c]);
        g_ch_pan[c] = p->pan[c] % 3;
        sound_set_pan(c + 1, pan_values[g_ch_pan[c] % 3]);
        if (g_ch_mute[c]) silence_channel(c);
        else apply_channel_volume(c);
    }
    apply_wave_ram(g_ch_tone[2]);
}

static void save_current_to_preset(int song_idx, int slot_idx) {
    if (song_idx < 0 || song_idx >= MIDI_SONG_COUNT) return;
    if (slot_idx < 0 || slot_idx >= MIDI_PRESETS_PER_SONG) return;
    SongPreset* p = &g_song_presets[song_idx][slot_idx];
    for (int c = 0; c < 6; c++) {
        p->track_mask[c] = g_ch_track_mask[c];
        p->tone[c] = (u8)(g_ch_tone[c] % get_num_tones(c));
        p->mute[c] = g_ch_mute[c] ? 1 : 0;
        p->octave[c] = g_ch_octave[c];
        p->vol[c] = g_ch_vol[c];
        p->pan[c] = g_ch_pan[c] % 3;
    }
    p->is_valid = 1;
    p->reserved = 0;
    midi_save_config();
}

static void clear_preset(int song_idx, int slot_idx) {
    if (song_idx < 0 || song_idx >= MIDI_SONG_COUNT) return;
    if (slot_idx < 0 || slot_idx >= MIDI_PRESETS_PER_SONG) return;
    u8* dst = (u8*)&g_song_presets[song_idx][slot_idx];
    for (int i = 0; i < (int)sizeof(SongPreset); i++) dst[i] = 0;
    midi_save_config();
}

/* --- Playback Status Badge Helper (Green Play Triangle ▶ / Red Mute Square ■ / Golden Solo 'S') --- */
static void draw_playback_badge(int x, int y, int ch, u16 bg_col) {
    gfx_draw_rect(x, y, 7, 7, bg_col);
    if (g_ch_solo[ch]) {
        /* Golden Solo Badge: Gold background + black 'S' */
        gfx_draw_rect(x, y, 7, 7, COLOR_GOLD);
        gfx_draw_char(x + 1, y, 'S', COLOR_BLACK, COLOR_GOLD, false);
    } else if (g_ch_mute[ch]) {
        /* Red Mute stop square (■): 5x5 filled square */
        gfx_draw_rect(x + 1, y + 1, 5, 5, COLOR_RED);
    } else if (has_any_solo() && !g_ch_solo[ch]) {
        /* Suppressed by solo mode: Dark gray stop square */
        gfx_draw_rect(x + 1, y + 1, 5, 5, COLOR_DARK_GRAY);
    } else {
        /* Green Play button triangle (▶): 5px wide, 7px high */
        gfx_draw_line_v(x + 1, y,     7, COLOR_EMERALD);
        gfx_draw_line_v(x + 2, y + 1, 5, COLOR_EMERALD);
        gfx_draw_line_v(x + 3, y + 2, 3, COLOR_EMERALD);
        gfx_draw_line_v(x + 4, y + 3, 1, COLOR_EMERALD);
    }
}

/* --- 6-Octave Piano Roll, Waterfall & 6-Channel VU Meters --- */
#define WATERFALL_LOOKAHEAD_FRAMES 28

static int  s_active_key_x[6] = { -1, -1, -1, -1, -1, -1 };
static bool s_active_is_black[6] = { false, false, false, false, false, false };
static u8   s_vu_level[6] = { 0, 0, 0, 0, 0, 0 };

static u16  g_waterfall_event_idx = 0;
static u8   s_waterfall_active_note[6] = { 0, 0, 0, 0, 0, 0 };
static u8   s_waterfall_hold[6] = { 0, 0, 0, 0, 0, 0 };

static int get_note_key_x(int midi_note, bool* is_black) {
    if (midi_note < 24) midi_note = 24;
    if (midi_note > 95) midi_note = 95;
    int r = midi_note - 24;
    int o = r / 12;
    int s = r % 12;
    static const int semitone_offsets[12] = { 0, 3, 5, 8, 10, 15, 18, 20, 23, 25, 28, 30 };
    static const bool black_flags[12] = { false, true, false, true, false, false, true, false, true, false, true, false };
    if (is_black) *is_black = black_flags[s];
    return 26 + o * 35 + semitone_offsets[s];
}

static u16 get_active_black_key_color(int bkx) {
    for (int c = 0; c < 6; c++) {
        if (g_ch_vis_note[c] > 0 && is_channel_audible(c)) {
            int shifted = (int)g_ch_vis_note[c] + (g_ch_octave[c] * 12);
            bool is_blk = false;
            int kx = get_note_key_x(shifted, &is_blk);
            if (kx == bkx && is_blk) return ch_colors[c];
        }
    }
    return COLOR_BLACK;
}

static void draw_piano_key(int kx, bool is_black, u16 color) {
    if (is_black) {
        gfx_draw_rect(kx, 128, 3, 11, color);
    } else {
        /* Full height white key: y = 128..147 (h = 20), w = 4 */
        gfx_draw_rect(kx, 128, 4, 20, color);
        gfx_draw_rect(kx + 4, 128, 1, 20, COLOR_DARK_GRAY);

        int rel = (kx - 26) % 35;
        /* Restore adjacent black key to the left if one exists */
        if (rel == 5 || rel == 10 || rel == 20 || rel == 25 || rel == 30) {
            int left_bkx = kx - 2;
            if (left_bkx >= 26) {
                gfx_draw_rect(left_bkx, 128, 3, 11, get_active_black_key_color(left_bkx));
            }
        }
        /* Restore adjacent black key to the right if one exists */
        if (rel == 0 || rel == 5 || rel == 15 || rel == 20 || rel == 25) {
            int right_bkx = kx + 3;
            if (right_bkx <= 232) {
                gfx_draw_rect(right_bkx, 128, 3, 11, get_active_black_key_color(right_bkx));
            }
        }
    }
}

static void draw_piano_base(void) {
    /* Framing lines */
    gfx_draw_rect(4, 98, 232, 1, COLOR_CARD_BORDER);
    gfx_draw_rect(4, 148, 232, 1, COLOR_CARD_BORDER);

    /* 6-Channel Dynamic Activity VU Meter Box on left side: x = 4..24, y = 99..147 */
    gfx_draw_rect(4, 99, 21, 49, COLOR_CARD_BG);
    gfx_draw_rect(4, 99, 21, 1, COLOR_CARD_BORDER);
    gfx_draw_rect(4, 99, 1, 49, COLOR_CARD_BORDER);
    gfx_draw_rect(24, 99, 1, 49, COLOR_CARD_BORDER);
    gfx_draw_rect(4, 147, 21, 1, COLOR_CARD_BORDER);

    /* VU Header */
    gfx_draw_text(9, 100, "VU", COLOR_CYAN_GLOW, COLOR_CARD_BG, false);
    gfx_draw_line_h(4, 108, 21, COLOR_DARK_GRAY);

    /* 6 Channel Labels at bottom: 2 cleanly spaced rows without ANY overlap */
    /* Row 1 (y = 135): PSG Pulse 1, Pulse 2, Wave */
    gfx_draw_char(5,  135, '1', ch_colors[0], COLOR_CARD_BG, false);
    gfx_draw_char(11, 135, '2', ch_colors[1], COLOR_CARD_BG, false);
    gfx_draw_char(17, 135, '3', ch_colors[2], COLOR_CARD_BG, false);

    /* Row 2 (y = 141): Noise Drum, DirectSound A, DirectSound B */
    gfx_draw_char(5,  141, '4', ch_colors[3], COLOR_CARD_BG, false);
    gfx_draw_char(11, 141, 'A', ch_colors[4], COLOR_CARD_BG, false);
    gfx_draw_char(17, 141, 'B', ch_colors[5], COLOR_CARD_BG, false);

    /* 42 white keys (5px wide: 4px white + 1px dark divider) spanning x = 26..235, y = 128..147 */
    for (int w = 0; w < 42; w++) {
        int kx = 26 + w * 5;
        gfx_draw_rect(kx, 128, 4, 20, COLOR_WHITE);
        gfx_draw_rect(kx + 4, 128, 1, 20, COLOR_DARK_GRAY);
    }

    /* 6 Octaves of Black keys (3px wide, 11px high) */
    for (int o = 0; o < 6; o++) {
        int base = 26 + o * 35;
        static const int b_offsets[5] = { 3, 8, 18, 23, 28 };
        for (int b = 0; b < 5; b++) {
            gfx_draw_rect(base + b_offsets[b], 128, 3, 11, COLOR_BLACK);
        }
    }

    /* Right spacer: x = 236 */
    gfx_draw_rect(236, 99, 1, 49, COLOR_CARD_BORDER);
}

static void reset_piano_visualizer_state(void) {
    for (int c = 0; c < 6; c++) {
        s_active_key_x[c] = -1;
        s_active_is_black[c] = false;
        s_vu_level[c] = 0;
        g_ch_vis_note[c] = 0;
        g_ch_vis_hold[c] = 0;
    }
    draw_piano_base();
}

static void step_waterfall_lookahead(void) {
    if (!g_is_playing) return;
    const SongInfo* song = &g_midi_songs[g_cur_song_idx];
    u32 lookahead_frame = g_cur_frame + WATERFALL_LOOKAHEAD_FRAMES;

    while (g_waterfall_event_idx < song->event_count && song->events[g_waterfall_event_idx].frame <= lookahead_frame) {
        const MidiEvent* ev = &song->events[g_waterfall_event_idx];
        u8 trk = ev->track;
        bool is_off = (ev->note & 0x80) != 0;
        u8 raw_note = ev->note & 0x7F;

        for (int c = 0; c < 6; c++) {
            if (!is_channel_audible(c)) continue;
            if (!(g_ch_track_mask[c] & (1 << trk))) continue;

            if (is_off) {
                if (s_waterfall_active_note[c] == raw_note) {
                    s_waterfall_active_note[c] = 0;
                }
            } else {
                s_waterfall_active_note[c] = raw_note;
                s_waterfall_hold[c] = 4;
            }
        }
        g_waterfall_event_idx++;
    }

    for (int c = 0; c < 6; c++) {
        if (s_waterfall_hold[c] > 0) {
            s_waterfall_hold[c]--;
        }
    }
}

static void reset_waterfall(void) {
    if (g_modal_depth == 0) {
        vu16* vram = (vu16*)MEM_VRAM;
        for (int y = 99; y <= 127; y++) {
            vu32* row = (vu32*)&vram[y * 240 + 26];
            for (int w = 0; w < 105; w++) row[w] = 0;
        }
    }
    g_waterfall_event_idx = 0;
    for (int c = 0; c < 6; c++) {
        s_waterfall_active_note[c] = 0;
        s_waterfall_hold[c] = 0;
    }
    if (g_modal_depth > 0) return;

    /* Pre-draw falling notes for the first 28 frames */
    const SongInfo* song = &g_midi_songs[g_cur_song_idx];
    while (g_waterfall_event_idx < song->event_count && song->events[g_waterfall_event_idx].frame <= WATERFALL_LOOKAHEAD_FRAMES) {
        const MidiEvent* ev = &song->events[g_waterfall_event_idx];
        u8 trk = ev->track;
        bool is_off = (ev->note & 0x80) != 0;
        u8 raw_note = ev->note & 0x7F;
        if (!is_off) {
            for (int c = 0; c < 6; c++) {
                if (!is_channel_audible(c)) continue;
                if (!(g_ch_track_mask[c] & (1 << trk))) continue;
                int shifted = (int)raw_note + (g_ch_octave[c] * 12);
                bool dummy = false;
                int kx = get_note_key_x(shifted, &dummy);
                int dy = 127 - (int)ev->frame;
                if (dy >= 99 && dy <= 127 && kx >= 26 && kx <= 232) {
                    gfx_draw_rect(kx, dy, 3, 2, ch_colors[c]);
                }
            }
        }
        g_waterfall_event_idx++;
    }
}

static void catchup_waterfall(void) {
    if (g_modal_depth == 0) {
        vu16* vram = (vu16*)MEM_VRAM;
        for (int y = 99; y <= 127; y++) {
            vu32* row = (vu32*)&vram[y * 240 + 26];
            for (int w = 0; w < 105; w++) row[w] = 0;
        }
    }
    if (g_modal_depth > 0) return;
    const SongInfo* song = &g_midi_songs[g_cur_song_idx];
    u32 target = g_cur_frame + WATERFALL_LOOKAHEAD_FRAMES;
    g_waterfall_event_idx = g_event_idx;
    while (g_waterfall_event_idx < song->event_count && song->events[g_waterfall_event_idx].frame <= target) {
        const MidiEvent* ev = &song->events[g_waterfall_event_idx];
        u8 trk = ev->track;
        bool is_off = (ev->note & 0x80) != 0;
        u8 raw_note = ev->note & 0x7F;
        if (!is_off) {
            for (int c = 0; c < 6; c++) {
                if (!is_channel_audible(c)) continue;
                if (!(g_ch_track_mask[c] & (1 << trk))) continue;
                int shifted = (int)raw_note + (g_ch_octave[c] * 12);
                bool dummy = false;
                int kx = get_note_key_x(shifted, &dummy);
                int dy = 127 - (int)(ev->frame - g_cur_frame);
                if (dy >= 99 && dy <= 127 && kx >= 26 && kx <= 232) {
                    gfx_draw_rect(kx, dy, 3, 2, ch_colors[c]);
                }
            }
        }
        g_waterfall_event_idx++;
    }
    for (int c = 0; c < 6; c++) {
        s_waterfall_active_note[c] = 0;
        s_waterfall_hold[c] = 0;
    }
}

static void update_piano_visualizer(void) {
    if (!g_is_playing) {
        for (int c = 0; c < 6; c++) {
            if (s_vu_level[c] > 0) {
                s_vu_level[c] = 0;
                int vx = 6 + c * 3;
                gfx_draw_rect(vx, 110, 2, 24, COLOR_CARD_BG);
            }
        }
        return;
    }

    /* 1. Step lookahead for upcoming notes (28 frames ahead) */
    step_waterfall_lookahead();

    vu16* vram = (vu16*)MEM_VRAM;

    /* 2. Fast 32-bit waterfall scroll across all 210 pixels (105 32-bit words, x = 26 to 235) */
    for (int y = 127; y >= 100; y--) {
        vu32* dst = (vu32*)&vram[y * 240 + 26];
        const vu32* src = (const vu32*)&vram[(y - 1) * 240 + 26];
        for (int w = 0; w < 105; w++) dst[w] = src[w];
    }
    vu32* top = (vu32*)&vram[99 * 240 + 26];
    for (int w = 0; w < 105; w++) top[w] = 0;

    /* 3. Inject active lookahead notes into top line of waterfall for ALL 6 CHANNELS! */
    for (int c = 0; c < 6; c++) {
        u8 note = s_waterfall_active_note[c];
        if (note > 0 && is_channel_audible(c)) {
            int shifted = (int)note + (g_ch_octave[c] * 12);
            bool dummy = false;
            int kx = get_note_key_x(shifted, &dummy);
            if (kx >= 26 && kx <= 232) {
                gfx_draw_rect(kx, 99, 3, 1, ch_colors[c]);
            }
        }
    }

    /* 4. Visual note hold decay for keys */
    for (int c = 0; c < 6; c++) {
        if (g_ch_vis_hold[c] > 0) {
            g_ch_vis_hold[c]--;
        } else if (g_ch_cur_note[c] == 0) {
            g_ch_vis_note[c] = 0;
        }
    }

    /* 5. Ultra-fast Full-Height Key Highlight & Multi-Channel Aware Restore */
    for (int c = 0; c < 6; c++) {
        int cur_kx = -1;
        bool cur_black = false;
        if (g_ch_vis_note[c] > 0 && is_channel_audible(c)) {
            int shifted = (int)g_ch_vis_note[c] + (g_ch_octave[c] * 12);
            cur_kx = get_note_key_x(shifted, &cur_black);
        }

        if (s_active_key_x[c] >= 0 && s_active_key_x[c] != cur_kx) {
            /* Note ended or changed: check if another channel is still using this key */
            bool key_shared = false;
            for (int o = 0; o < 6; o++) {
                if (o != c && g_ch_vis_note[o] > 0 && is_channel_audible(o)) {
                    int o_sh = (int)g_ch_vis_note[o] + (g_ch_octave[o] * 12);
                    bool o_blk = false;
                    int o_kx = get_note_key_x(o_sh, &o_blk);
                    if (o_kx == s_active_key_x[c]) {
                        key_shared = true;
                        draw_piano_key(s_active_key_x[c], s_active_is_black[c], ch_colors[o]);
                        break;
                    }
                }
            }
            if (!key_shared) {
                draw_piano_key(s_active_key_x[c], s_active_is_black[c], s_active_is_black[c] ? COLOR_BLACK : COLOR_WHITE);
            }
        }

        if (cur_kx >= 0 && cur_kx != s_active_key_x[c]) {
            draw_piano_key(cur_kx, cur_black, ch_colors[c]);
        }

        s_active_key_x[c] = cur_kx;
        s_active_is_black[c] = cur_black;
    }

    /* 6. Real-Time 6-Channel Activity VU Meters (Flicker-Free, No Stuck Pixels) */
    for (int c = 0; c < 6; c++) {
        int cur_lvl = s_vu_level[c];
        int target_lvl = 0;
        if (g_ch_vis_note[c] > 0 && is_channel_audible(c)) {
            target_lvl = (24 * g_ch_vol[c] + 50) / 100; /* Scale 0..24 peak height by channel volume */
            if (target_lvl > 24) target_lvl = 24;
        }
        if (target_lvl > cur_lvl) {
            cur_lvl = target_lvl;
        } else if (cur_lvl > target_lvl) {
            cur_lvl -= 2; /* Smooth natural decay */
            if (cur_lvl < target_lvl) cur_lvl = target_lvl;
        }
        if (cur_lvl < 0) cur_lvl = 0;
        if (cur_lvl > 24) cur_lvl = 24;
        s_vu_level[c] = (u8)cur_lvl;

        int vx = 6 + c * 3;
        int empty_h = 24 - cur_lvl;
        if (empty_h > 0) {
            gfx_draw_rect(vx, 110, 2, empty_h, COLOR_CARD_BG);
        }
        if (cur_lvl > 0) {
            gfx_draw_rect(vx, 110 + empty_h, 2, cur_lvl, ch_colors[c]);
        }
    }
}

/* --- Real-Time Background Sequencer Step --- */
static void step_sequencer(void) {
    if (!g_is_playing) return;

    /* Auto-stop DirectSound notes whose samples have finished playing */
    if (g_ds_frames_left[0] > 0) {
        g_ds_frames_left[0]--;
        if (g_ds_frames_left[0] == 0) {
            sound_stop_directsound_a();
            if ((g_ch_tone[4] % NUM_TONES_DS) > 0) {
                g_ch_cur_note[4] = 0;
            }
        }
    }
    if (g_ds_frames_left[1] > 0) {
        g_ds_frames_left[1]--;
        if (g_ds_frames_left[1] == 0) {
            sound_stop_directsound_b();
            if ((g_ch_tone[5] % NUM_TONES_DS) > 0) {
                g_ch_cur_note[5] = 0;
            }
        }
    }

    const SongInfo* song = &g_midi_songs[g_cur_song_idx];
    while (g_event_idx < song->event_count && song->events[g_event_idx].frame <= g_cur_frame) {
        const MidiEvent* ev = &song->events[g_event_idx];
        u8 trk = ev->track;
        bool is_off = (ev->note & 0x80) != 0;
        u8 raw_note = ev->note & 0x7F;

        for (int c = 0; c < 6; c++) {
            if (!is_channel_audible(c)) continue;
            if (!(g_ch_track_mask[c] & (1 << trk))) continue;

            if (is_off) {
                /* Note-Off: Only silence if this channel is currently sounding this exact note */
                if (g_ch_cur_note[c] == raw_note) {
                    if (c < 4 || (g_ch_tone[c] % NUM_TONES_DS) > 0) {
                        silence_channel(c);
                        g_ch_cur_note[c] = 0;
                    } else {
                        /* For DirectSound PCM drums (preset 0), do NOT silence early and
                           do NOT clear g_ds_frames_left! Drums are one-shot percussion
                           samples that must play through to completion. step_sequencer()
                           will auto-stop the channel when g_ds_frames_left reaches 0. */
                        g_ch_cur_note[c] = 0;
                    }
                }
            } else {
                /* Note-On: Sound the note */
                g_ch_cur_note[c] = raw_note;
                g_ch_vis_note[c] = raw_note;
                g_ch_vis_hold[c] = 5;

                /* Apply per-channel octave shift */
                int shifted = (int)raw_note + (g_ch_octave[c] * 12);
                if (shifted < 1) shifted = 1;
                if (shifted > 127) shifted = 127;

                u16 gba_freq = midi_to_gba_freq[shifted];

                if (c == 0) {
                    int tone = g_ch_tone[0] % NUM_TONES_CH1;
                    u8 duty = ch1_duty[tone];
                    u8 env = ch1_env[tone];
                    u8 vol = (g_ch_vol[0] * 15 + 50) / 100;
                    REG_SOUND1CNT_L = ch1_sweep[tone];
                    REG_SOUND1CNT_H = (u16)(((duty & 3) << 6) | (vol << 12) | (env << 8));
                    REG_SOUND1CNT_X = (u16)(0x8000 | (gba_freq & 0x07FF));
                } else if (c == 1) {
                    int tone = g_ch_tone[1] % NUM_TONES_CH2;
                    u8 duty = ch2_duty[tone];
                    u8 env = ch2_env[tone];
                    u8 vol = (g_ch_vol[1] * 15 + 50) / 100;
                    REG_SOUND2CNT_L = (u16)(((duty & 3) << 6) | (vol << 12) | (env << 8));
                    REG_SOUND2CNT_H = (u16)(0x8000 | (gba_freq & 0x07FF));
                } else if (c == 2) {
                    REG_SOUND3CNT_L = 0x0080;
                    u16 w_vol;
                    if (g_ch_vol[2] == 0) w_vol = 0x0000;
                    else if (g_ch_vol[2] <= 35) w_vol = 0x6000;
                    else if (g_ch_vol[2] <= 65) w_vol = 0x4000;
                    else if (g_ch_vol[2] <= 85) w_vol = 0x8000;
                    else w_vol = 0x2000;
                    REG_SOUND3CNT_H = w_vol;
                    REG_SOUND3CNT_X = (u16)(0x8000 | (gba_freq & 0x07FF));
                } else if (c == 3) {
                    /* Advanced GBA PSG Noise, Percussion & Pitched 7-Bit Synthesizer */
                    int mode = g_ch_tone[3] % NUM_TONES_CH4;
                    u8 vol = (g_ch_vol[3] * 15 + 50) / 100;

                    if (mode == 4) {
                        /* Pitched 7-Bit Robotic Chiptune Bass */
                        int oct_shift = 8 - (shifted / 12);
                        if (oct_shift < 1) oct_shift = 1;
                        if (oct_shift > 13) oct_shift = 13;
                        u8 r = (shifted % 12 < 6) ? 3 : 2;
                        REG_SOUND4CNT_L = (vol << 12) | (3 << 8);
                        REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | r | (oct_shift << 4));
                    } else if (mode == 5) {
                        /* Pitched 7-Bit Metallic Cyber Lead */
                        int oct_shift = 6 - (shifted / 12);
                        if (oct_shift < 0) oct_shift = 0;
                        if (oct_shift > 11) oct_shift = 11;
                        u8 r = (shifted % 12 < 6) ? 2 : 1;
                        REG_SOUND4CNT_L = (vol << 12) | (2 << 8);
                        REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | r | (oct_shift << 4));
                    } else if (mode == 6) {
                        /* Hi-Tech Cyber Glitch & Laser Zaps */
                        u8 clk = (raw_note > 60) ? 1 : ((raw_note > 40) ? 2 : 4);
                        REG_SOUND4CNT_L = (vol << 12) | (1 << 8);
                        REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | 1 | (clk << 4));
                    } else if (mode == 7) {
                        /* Soft Lo-Fi Brushes & Shakers */
                        u8 s_vol = (vol > 4) ? (vol - 4) : 1;
                        REG_SOUND4CNT_L = (s_vol << 12) | (1 << 8);
                        REG_SOUND4CNT_H = 0x8000 | 0 | (1 << 4);
                    } else if (mode == 8) {
                        /* Power Industrial Techno Kick/Snare */
                        if (raw_note <= 36) {
                            REG_SOUND4CNT_L = (vol << 12) | (1 << 8);
                            REG_SOUND4CNT_H = 0x8000 | 1 | (7 << 4);
                        } else {
                            REG_SOUND4CNT_L = (vol << 12) | (3 << 8);
                            REG_SOUND4CNT_H = 0x8000 | (1 << 3) | 2 | (1 << 4);
                        }
                    } else if (mode == 9) {
                        /* Arcade Alien SFX & Explosions */
                        u8 s = (shifted % 7) + 1;
                        REG_SOUND4CNT_L = (vol << 12) | (6 << 8);
                        REG_SOUND4CNT_H = 0x8000 | (1 << 3) | 0 | (s << 4);
                    } else if (mode == 0) {
                        /* Punchy 808/909 Electronic Drum */
                        if (raw_note <= 36) {
                            /* 808 Sub Kick */
                            REG_SOUND4CNT_L = (vol << 12) | (1 << 8);
                            REG_SOUND4CNT_H = 0x8000 | 1 | (6 << 4);
                        } else if (raw_note <= 40) {
                            /* 909 Snare */
                            REG_SOUND4CNT_L = (vol << 12) | (2 << 8);
                            REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | 2 | (3 << 4));
                        } else if (raw_note <= 44) {
                            /* 808 Hi-Hat */
                            REG_SOUND4CNT_L = ((vol > 3 ? vol - 3 : 1) << 12) | (1 << 8);
                            REG_SOUND4CNT_H = 0x8000 | 0 | (1 << 4);
                        } else {
                            /* Crash / Cymbal */
                            REG_SOUND4CNT_L = (vol << 12) | (4 << 8);
                            REG_SOUND4CNT_H = 0x8000 | 1 | (2 << 4);
                        }
                    } else if (mode == 1) {
                        /* Acoustic Studio Drum Kit */
                        if (raw_note <= 36) {
                            /* Acoustic Kick */
                            REG_SOUND4CNT_L = (vol << 12) | (2 << 8);
                            REG_SOUND4CNT_H = 0x8000 | 3 | (5 << 4);
                        } else if (raw_note <= 40) {
                            /* Studio Snare */
                            REG_SOUND4CNT_L = (vol << 12) | (3 << 8);
                            REG_SOUND4CNT_H = 0x8000 | 2 | (3 << 4);
                        } else if (raw_note <= 44) {
                            /* Hi-Hat */
                            REG_SOUND4CNT_L = ((vol > 4 ? vol - 4 : 1) << 12) | (1 << 8);
                            REG_SOUND4CNT_H = 0x8000 | 1 | (1 << 4);
                        } else {
                            /* Ride / Crash */
                            REG_SOUND4CNT_L = (vol << 12) | (5 << 8);
                            REG_SOUND4CNT_H = 0x8000 | 2 | (2 << 4);
                        }
                    } else if (mode == 2) {
                        /* 8-Bit Retro Game Boy Chiptune Kit */
                        if (raw_note <= 36) {
                            /* Crunchy Chiptune Kick */
                            REG_SOUND4CNT_L = (vol << 12) | (1 << 8);
                            REG_SOUND4CNT_H = 0x8000 | (1 << 3) | 3 | (4 << 4);
                        } else if (raw_note <= 40) {
                            /* Retro Snare Crack */
                            REG_SOUND4CNT_L = (vol << 12) | (2 << 8);
                            REG_SOUND4CNT_H = 0x8000 | (1 << 3) | 1 | (2 << 4);
                        } else if (raw_note <= 44) {
                            /* Chiptune Hat */
                            REG_SOUND4CNT_L = ((vol > 4 ? vol - 4 : 1) << 12) | (1 << 8);
                            REG_SOUND4CNT_H = 0x8000 | (1 << 3) | 0 | (1 << 4);
                        } else {
                            /* Chiptune Zap / Cymbal */
                            REG_SOUND4CNT_L = (vol << 12) | (3 << 8);
                            REG_SOUND4CNT_H = 0x8000 | (1 << 3) | 2 | (2 << 4);
                        }
                    } else if (mode == 10) {
                        /* Dubstep Metallic Snare & Hat */
                        REG_SOUND4CNT_L = (vol << 12) | (2 << 8);
                        REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | 1 | (2 << 4));
                    } else if (mode == 11) {
                        /* Arcade Explosion & Sci-Fi SFX */
                        REG_SOUND4CNT_L = (vol << 12) | (7 << 8);
                        REG_SOUND4CNT_H = 0x8000 | 3 | (4 << 4);
                    } else {
                        /* Heavy Rock & Metal Kit */
                        if (raw_note <= 36) {
                            /* Heavy Kick */
                            REG_SOUND4CNT_L = (vol << 12) | (2 << 8);
                            REG_SOUND4CNT_H = 0x8000 | 2 | (6 << 4);
                        } else if (raw_note <= 40) {
                            /* Explosive Snare */
                            REG_SOUND4CNT_L = (vol << 12) | (5 << 8);
                            REG_SOUND4CNT_H = 0x8000 | (1 << 3) | 3 | (2 << 4);
                        } else if (raw_note <= 44) {
                            /* Rock Hat */
                            REG_SOUND4CNT_L = ((vol > 3 ? vol - 3 : 1) << 12) | (1 << 8);
                            REG_SOUND4CNT_H = 0x8000 | 1 | (1 << 4);
                        } else {
                            /* Heavy Crash */
                            REG_SOUND4CNT_L = (vol << 12) | (6 << 8);
                            REG_SOUND4CNT_H = 0x8000 | 2 | (1 << 4);
                        }
                    }
                } else if (c == 4) {
                    /* DirectSound A (DMA 1, Timer 0, FIFO A) */
                    int preset = g_ch_tone[4] % NUM_TONES_DS;
                    u8 pan = pan_values[g_ch_pan[4] % 3];
                    if (preset == 0) {
                        /* Real PCM Drum Kit */
                        int d_len = sizeof(ds_kick_pcm);
                        if (raw_note <= 36) {
                            d_len = sizeof(ds_kick_pcm);
                            sound_play_directsound_a(ds_kick_pcm, d_len, pan, g_ch_vol[4]);
                        } else if (raw_note == 38 || raw_note == 40) {
                            d_len = sizeof(ds_snare_pcm);
                            sound_play_directsound_a(ds_snare_pcm, d_len, pan, g_ch_vol[4]);
                        } else if (raw_note == 42 || raw_note == 44 || raw_note == 46) {
                            d_len = sizeof(ds_hihat_pcm);
                            sound_play_directsound_a(ds_hihat_pcm, d_len, pan, g_ch_vol[4]);
                        } else if (raw_note == 49 || raw_note == 52) {
                            d_len = sizeof(ds_crash_pcm);
                            sound_play_directsound_a(ds_crash_pcm, d_len, pan, g_ch_vol[4]);
                        } else if (raw_note == 51 || raw_note == 53 || raw_note == 59) {
                            d_len = sizeof(ds_ride_pcm);
                            sound_play_directsound_a(ds_ride_pcm, d_len, pan, g_ch_vol[4]);
                        } else if (raw_note == 55 || raw_note == 57 || raw_note == 58 || raw_note == 69 || raw_note == 70 || raw_note == 82) {
                            d_len = sizeof(ds_shaker_pcm);
                            sound_play_directsound_a(ds_shaker_pcm, d_len, pan, g_ch_vol[4]);
                        } else if (raw_note == 37 || raw_note == 39) {
                            d_len = sizeof(ds_clap_pcm);
                            sound_play_directsound_a(ds_clap_pcm, d_len, pan, g_ch_vol[4]);
                        } else if (raw_note >= 60 && raw_note <= 68) {
                            d_len = sizeof(ds_conga_pcm);
                            sound_play_directsound_a(ds_conga_pcm, d_len, pan, g_ch_vol[4]);
                        } else {
                            d_len = sizeof(ds_tom_pcm);
                            sound_play_directsound_a(ds_tom_pcm, d_len, pan, g_ch_vol[4]);
                        }
                        g_ds_frames_left[0] = (int)(((u64)d_len * 1024ULL * 60 + 16777215ULL) / 16777216ULL) + 1;
                    } else {
                        /* Melodic DirectSound Instrument */
                        /* Octave fold shifted note to safe playable musical range 24..84 (C1..C6)
                           to prevent ultra-high timer reload values (>= 0xFF00) that cause DMA
                           bus saturation and high-pitched squeak / interference artefacts. */
                        int ds_shifted = shifted;
                        while (ds_shifted > 84) ds_shifted -= 12;
                        while (ds_shifted < 24) ds_shifted += 12;
                        const s8* pcm = ds_melodic_samples[preset];
                        int len = ds_melodic_lengths[preset];
                        u16 tm = ds_note_timer_reload[ds_shifted];
                        sound_play_directsound_a_note(pcm, len, tm, pan, g_ch_vol[4]);
                        u32 cycles = 65536 - tm;
                        int dur = (int)(((u64)len * cycles * 60 + 16777215ULL) / 16777216ULL);
                        if (dur < 2) dur = 2;
                        g_ds_frames_left[0] = dur + 1;
                    }
                } else if (c == 5) {
                    /* DirectSound B (DMA 2, Timer 1, FIFO B) */
                    int preset = g_ch_tone[5] % NUM_TONES_DS;
                    u8 pan = pan_values[g_ch_pan[5] % 3];
                    if (preset == 0) {
                        /* Real PCM Drum Kit */
                        int d_len = sizeof(ds_kick_pcm);
                        if (raw_note <= 36) {
                            d_len = sizeof(ds_kick_pcm);
                            sound_play_directsound_b(ds_kick_pcm, d_len, pan, g_ch_vol[5]);
                        } else if (raw_note == 38 || raw_note == 40) {
                            d_len = sizeof(ds_snare_pcm);
                            sound_play_directsound_b(ds_snare_pcm, d_len, pan, g_ch_vol[5]);
                        } else if (raw_note == 42 || raw_note == 44 || raw_note == 46) {
                            d_len = sizeof(ds_hihat_pcm);
                            sound_play_directsound_b(ds_hihat_pcm, d_len, pan, g_ch_vol[5]);
                        } else if (raw_note == 49 || raw_note == 52) {
                            d_len = sizeof(ds_crash_pcm);
                            sound_play_directsound_b(ds_crash_pcm, d_len, pan, g_ch_vol[5]);
                        } else if (raw_note == 51 || raw_note == 53 || raw_note == 59) {
                            d_len = sizeof(ds_ride_pcm);
                            sound_play_directsound_b(ds_ride_pcm, d_len, pan, g_ch_vol[5]);
                        } else if (raw_note == 55 || raw_note == 57 || raw_note == 58 || raw_note == 69 || raw_note == 70 || raw_note == 82) {
                            d_len = sizeof(ds_shaker_pcm);
                            sound_play_directsound_b(ds_shaker_pcm, d_len, pan, g_ch_vol[5]);
                        } else if (raw_note == 37 || raw_note == 39) {
                            d_len = sizeof(ds_clap_pcm);
                            sound_play_directsound_b(ds_clap_pcm, d_len, pan, g_ch_vol[5]);
                        } else if (raw_note >= 60 && raw_note <= 68) {
                            d_len = sizeof(ds_conga_pcm);
                            sound_play_directsound_b(ds_conga_pcm, d_len, pan, g_ch_vol[5]);
                        } else {
                            d_len = sizeof(ds_tom_pcm);
                            sound_play_directsound_b(ds_tom_pcm, d_len, pan, g_ch_vol[5]);
                        }
                        g_ds_frames_left[1] = (int)(((u64)d_len * 1024ULL * 60 + 16777215ULL) / 16777216ULL) + 1;
                    } else {
                        /* Melodic DirectSound Instrument */
                        /* Octave fold shifted note to safe playable musical range 24..84 (C1..C6)
                           to prevent ultra-high timer reload values (>= 0xFF00) that cause DMA
                           bus saturation and high-pitched squeak / interference artefacts. */
                        int ds_shifted = shifted;
                        while (ds_shifted > 84) ds_shifted -= 12;
                        while (ds_shifted < 24) ds_shifted += 12;
                        const s8* pcm = ds_melodic_samples[preset];
                        int len = ds_melodic_lengths[preset];
                        u16 tm = ds_note_timer_reload[ds_shifted];
                        sound_play_directsound_b_note(pcm, len, tm, pan, g_ch_vol[5]);
                        u32 cycles = 65536 - tm;
                        int dur = (int)(((u64)len * cycles * 60 + 16777215ULL) / 16777216ULL);
                        if (dur < 2) dur = 2;
                        g_ds_frames_left[1] = dur + 1;
                    }
                }
            }
        }
        g_event_idx++;
    }

    g_cur_frame++;
    if (g_cur_frame >= song->total_frames) {
        g_cur_frame = 0;
        g_event_idx = 0;
        sound_stop_directsound();
        g_ds_frames_left[0] = 0;
        g_ds_frames_left[1] = 0;
        reset_waterfall();
    }
}

/* --- Modal Window 1: Song Selection Menu (Flicker-Free Live Playback!) --- */
static void draw_song_select_row(int song_idx, int slot_idx, int current_song, bool is_sel) {
    int y = 31 + slot_idx * 20;
    if (song_idx >= MIDI_SONG_COUNT) {
        gfx_draw_rect(14, y - 1, 212, 19, COLOR_CARD_BG);
        return;
    }

    bool is_active_playing = (song_idx == current_song);
    u16 bg_col = is_sel ? COLOR_CARD_BORDER : COLOR_CARD_BG;

    gfx_draw_rect(14, y - 1, 212, 19, bg_col);

    u16 title_col = is_active_playing ? COLOR_EMERALD : (is_sel ? COLOR_GOLD : COLOR_WHITE);
    gfx_draw_text_fmt(18, y, title_col, bg_col, false,
                      "%s [%02d] %s",
                      is_sel ? ">" : " ",
                      song_idx + 1,
                      g_midi_songs[song_idx].title);

    int sec = g_midi_songs[song_idx].total_frames / 60;
    gfx_draw_text_fmt(28, y + 9, is_sel ? COLOR_CYAN : COLOR_LIGHT_GRAY, bg_col, false,
                      "%d BPM | %d Sp | %02d:%02d | %s",
                      g_midi_songs[song_idx].bpm,
                      g_midi_songs[song_idx].track_count,
                      sec / 60, sec % 60,
                      g_midi_songs[song_idx].subtitle);
}

static void draw_song_select_page(int scroll_top, int sel, int current_song) {
    const int page_size = 5;
    for (int i = 0; i < page_size; i++) {
        draw_song_select_row(scroll_top + i, i, current_song, (scroll_top + i) == sel);
    }
    gfx_draw_text(218, 31, (scroll_top > 0) ? "^" : " ", COLOR_CYAN, COLOR_CARD_BG, false);
    gfx_draw_text(218, 118, (scroll_top + page_size < MIDI_SONG_COUNT) ? "v" : " ", COLOR_CYAN, COLOR_CARD_BG, false);
}

static int open_song_select_modal(int current_song) {
    g_modal_depth++;
    int sel = current_song % MIDI_SONG_COUNT;
    int scroll_top = 0;
    const int page_size = 5;

    if (sel >= page_size) {
        scroll_top = sel - page_size + 1;
        if (scroll_top > MIDI_SONG_COUNT - page_size) {
            scroll_top = MIDI_SONG_COUNT - page_size;
        }
    }

    /* Draw static dialog frame ONCE to prevent menu flickering */
    gfx_draw_rect(10, 14, 220, 132, COLOR_CARD_BG);
    gfx_draw_rect(10, 14, 220, 1, COLOR_CYAN);
    gfx_draw_rect(10, 145, 220, 1, COLOR_CYAN);
    gfx_draw_rect(10, 14, 1, 132, COLOR_CYAN);
    gfx_draw_rect(229, 14, 1, 132, COLOR_CYAN);

    gfx_draw_text_fmt(16, 18, COLOR_GOLD, COLOR_CARD_BG, false, "SELECT MIDI SONG (%d TOTAL)", MIDI_SONG_COUNT);
    gfx_draw_rect(14, 28, 212, 1, COLOR_DARK_GRAY);
    gfx_draw_rect(14, 132, 212, 1, COLOR_DARK_GRAY);
    gfx_draw_text(16, 135, "UP/DN: Scroll | A: Load Song | B: Cancel", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);

    draw_song_select_page(scroll_top, sel, current_song);

    while (1) {
        vsync();
        input_poll();

        /* LIVE BACKGROUND PLAYBACK */
        step_sequencer();

        if (key_hit(KEY_B)) {
            g_modal_depth--;
            return current_song; /* Cancel */
        }
        if (key_hit(KEY_A)) {
            g_modal_depth--;
            return sel; /* Confirm selection */
        }

        if (key_hit(KEY_UP)) {
            int prev = sel;
            sel = (sel - 1 + MIDI_SONG_COUNT) % MIDI_SONG_COUNT;
            int prev_scroll = scroll_top;
            if (sel < scroll_top) {
                scroll_top = sel;
            } else if (sel >= scroll_top + page_size) {
                scroll_top = sel - page_size + 1;
            }

            if (scroll_top != prev_scroll) {
                draw_song_select_page(scroll_top, sel, current_song);
            } else {
                draw_song_select_row(prev, prev - scroll_top, current_song, false);
                draw_song_select_row(sel, sel - scroll_top, current_song, true);
            }
        }
        if (key_hit(KEY_DOWN)) {
            int prev = sel;
            sel = (sel + 1) % MIDI_SONG_COUNT;
            int prev_scroll = scroll_top;
            if (sel < scroll_top) {
                scroll_top = sel;
            } else if (sel >= scroll_top + page_size) {
                scroll_top = sel - page_size + 1;
            }

            if (scroll_top != prev_scroll) {
                draw_song_select_page(scroll_top, sel, current_song);
            } else {
                draw_song_select_row(prev, prev - scroll_top, current_song, false);
                draw_song_select_row(sel, sel - scroll_top, current_song, true);
            }
        }
    }
}

/* --- Modal Window 2: Multi-Track Assignment Menu (Flicker-Free!) --- */
static void draw_track_select_row(int ch, int opt_idx, int slot_idx, u16 mask, bool is_sel, const SongInfo* song) {
    int y = 32 + slot_idx * 14;
    int total_opts = 1 + song->track_count;
    if (opt_idx >= total_opts) {
        gfx_draw_rect(14, y - 1, 212, 13, COLOR_CARD_BG);
        return;
    }

    u16 bg_col = is_sel ? COLOR_CARD_BORDER : COLOR_CARD_BG;

    gfx_draw_rect(14, y - 1, 212, 13, bg_col);

    if (opt_idx == 0) {
        bool all_clear = (mask == 0);
        u16 clr_col = all_clear ? COLOR_RED : (is_sel ? COLOR_CORAL : COLOR_DARK_GRAY);
        gfx_draw_text_fmt(18, y, clr_col, bg_col, false,
                          "%s [%s] CLEAR ALL / MUTE %s",
                          is_sel ? ">" : " ",
                          all_clear ? "X" : " ",
                          ch_names[ch]);
    } else {
        int trk_idx = opt_idx - 1;
        const MidiTrackDesc* tdesc = &song->tracks[trk_idx];
        bool is_on = (mask & (1 << trk_idx)) != 0;

        /* Check assignments across all channels */
        int other_ch = -1;
        int other_cnt = 0;
        for (int c = 0; c < 6; c++) {
            if (c != ch && (g_ch_track_mask[c] & (1 << trk_idx))) {
                if (other_ch < 0) other_ch = c;
                other_cnt++;
            }
        }
        bool is_free = (!is_on && other_cnt == 0);

        if (is_free) {
            gfx_draw_text_fmt(18, y, is_sel ? COLOR_GOLD : COLOR_LIGHT_GRAY, bg_col, false,
                              "%s [ ] T%d: %-10.10s [FREI]",
                              is_sel ? ">" : " ",
                              trk_idx + 1,
                              tdesc->name);
        } else if (is_on) {
            if (other_cnt > 0) {
                gfx_draw_text_fmt(18, y, COLOR_EMERALD, bg_col, false,
                                  "%s [*] T%d: %-10.10s (+%s)",
                                  is_sel ? ">" : " ",
                                  trk_idx + 1,
                                  tdesc->name,
                                  ch_names[other_ch]);
            } else {
                gfx_draw_text_fmt(18, y, COLOR_EMERALD, bg_col, false,
                                  "%s [*] T%d: %-10.10s (%un)",
                                  is_sel ? ">" : " ",
                                  trk_idx + 1,
                                  tdesc->name,
                                  tdesc->note_count);
            }
        } else {
            /* Assigned to another channel */
            gfx_draw_text_fmt(18, y, is_sel ? COLOR_GOLD : COLOR_CYAN, bg_col, false,
                              "%s [ ] T%d: %-10.10s (%s%s)",
                              is_sel ? ">" : " ",
                              trk_idx + 1,
                              tdesc->name,
                              ch_names[other_ch],
                              (other_cnt > 1) ? "+" : "");
        }
    }
}

static void draw_track_select_page(int ch, int scroll_top, int sel, u16 mask, const SongInfo* song) {
    const int page_size = 7;
    int total_opts = 1 + song->track_count;
    for (int i = 0; i < page_size; i++) {
        draw_track_select_row(ch, scroll_top + i, i, mask, (scroll_top + i) == sel, song);
    }
    gfx_draw_text(216, 32, (scroll_top > 0) ? "^" : " ", COLOR_CYAN, COLOR_CARD_BG, false);
    gfx_draw_text(216, 116, (scroll_top + page_size < total_opts) ? "v" : " ", COLOR_CYAN, COLOR_CARD_BG, false);
}

static u16 open_track_select_modal(int ch, u16 current_mask, const SongInfo* song) {
    g_modal_depth++;
    int total_opts = 1 + song->track_count;
    int sel = 0;
    u16 mask = current_mask;
    int scroll_top = 0;
    const int page_size = 7;

    /* Draw static frame ONCE */
    gfx_draw_rect(10, 14, 220, 132, COLOR_CARD_BG);
    gfx_draw_rect(10, 14, 220, 1, ch_colors[ch]);
    gfx_draw_rect(10, 145, 220, 1, ch_colors[ch]);
    gfx_draw_rect(10, 14, 1, 132, ch_colors[ch]);
    gfx_draw_rect(229, 14, 1, 132, ch_colors[ch]);

    gfx_draw_text_fmt(16, 18, ch_colors[ch], COLOR_CARD_BG, false, "ASSIGN SPUREN TO %s (TOGGLE MULTI)", ch_names[ch]);
    gfx_draw_rect(14, 28, 212, 1, COLOR_DARK_GRAY);
    gfx_draw_rect(14, 132, 212, 1, COLOR_DARK_GRAY);
    gfx_draw_text(16, 135, "A: Toggle On/Off | B: Done & Apply", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);

    draw_track_select_page(ch, scroll_top, sel, mask, song);

    while (1) {
        vsync();
        input_poll();

        /* LIVE BACKGROUND PLAYBACK */
        step_sequencer();

        if (key_hit(KEY_B)) {
            g_modal_depth--;
            return mask; /* Done & apply */
        }

        if (key_hit(KEY_A)) {
            if (sel == 0) {
                mask = 0;
            } else {
                int trk_idx = sel - 1;
                mask ^= (1 << trk_idx);
            }
            /* Always redraw entire visible page so Clear All and multi-channel statuses update immediately */
            draw_track_select_page(ch, scroll_top, sel, mask, song);
        }

        if (key_hit(KEY_UP)) {
            int prev = sel;
            sel = (sel - 1 + total_opts) % total_opts;
            int prev_scroll = scroll_top;
            if (sel < scroll_top) scroll_top = sel;
            else if (sel >= scroll_top + page_size) scroll_top = sel - page_size + 1;

            if (scroll_top != prev_scroll) {
                draw_track_select_page(ch, scroll_top, sel, mask, song);
            } else {
                draw_track_select_row(ch, prev, prev - scroll_top, mask, false, song);
                draw_track_select_row(ch, sel, sel - scroll_top, mask, true, song);
            }
        }
        if (key_hit(KEY_DOWN)) {
            int prev = sel;
            sel = (sel + 1) % total_opts;
            int prev_scroll = scroll_top;
            if (sel < scroll_top) scroll_top = sel;
            else if (sel >= scroll_top + page_size) scroll_top = sel - page_size + 1;

            if (scroll_top != prev_scroll) {
                draw_track_select_page(ch, scroll_top, sel, mask, song);
            } else {
                draw_track_select_row(ch, prev, prev - scroll_top, mask, false, song);
                draw_track_select_row(ch, sel, sel - scroll_top, mask, true, song);
            }
        }
    }
}

static void retune_active_channel_note(int ch) {
    if (g_ch_cur_note[ch] > 0 && !g_ch_mute[ch]) {
        int shifted = (int)g_ch_cur_note[ch] + (g_ch_octave[ch] * 12);
        if (shifted < 1) shifted = 1;
        if (shifted > 127) shifted = 127;
        if (ch < 3) {
            u16 f = midi_to_gba_freq[shifted];
            if (ch == 0) REG_SOUND1CNT_X = (u16)(0x8000 | (f & 0x07FF));
            else if (ch == 1) REG_SOUND2CNT_H = (u16)(0x8000 | (f & 0x07FF));
            else if (ch == 2) REG_SOUND3CNT_X = (u16)(0x8000 | (f & 0x07FF));
        } else if (ch == 4 && (g_ch_tone[4] % NUM_TONES_DS) > 0) {
            int s = shifted;
            while (s > 84) s -= 12;
            while (s < 24) s += 12;
            REG_TM0CNT_L = ds_note_timer_reload[s];
        } else if (ch == 5 && (g_ch_tone[5] % NUM_TONES_DS) > 0) {
            int s = shifted;
            while (s > 84) s -= 12;
            while (s < 24) s += 12;
            REG_TM1CNT_L = ds_note_timer_reload[s];
        }
    }
}

/* --- Modal Window 4: Sound / Instrument Selection Submenu (Flicker-Free With Live Audition & Scrolling!) --- */
static void draw_sound_select_row(int ch, int opt_idx, int slot_idx, int current_sound, bool is_sel) {
    int y = 28 + slot_idx * 10;
    int num_tones = get_num_tones(ch);
    if (opt_idx >= num_tones) {
        gfx_draw_rect(14, y - 1, 212, 10, COLOR_CARD_BG);
        return;
    }

    u16 bg_col = is_sel ? COLOR_CARD_BORDER : COLOR_CARD_BG;
    u16 fg_col = is_sel ? COLOR_GOLD : (opt_idx == current_sound ? COLOR_CYAN : COLOR_WHITE);

    gfx_draw_rect(14, y - 1, 212, 10, bg_col);

    const char* desc = "";
    if (ch == 0) desc = ch1_tones[opt_idx];
    else if (ch == 1) desc = ch2_tones[opt_idx];
    else if (ch == 2) desc = ch3_tones[opt_idx];
    else if (ch == 3) desc = ch4_tones[opt_idx];
    else desc = ds_tones[opt_idx];

    gfx_draw_text_fmt(16, y, fg_col, bg_col, false,
                      "%s%2d. %s",
                      is_sel ? ">" : " ",
                      opt_idx + 1,
                      desc);
}

static void draw_sound_select_page(int ch, int scroll_top, int sel, int current_sound) {
    const int page_size = 10;
    int num_tones = get_num_tones(ch);
    for (int i = 0; i < page_size; i++) {
        draw_sound_select_row(ch, scroll_top + i, i, current_sound, (scroll_top + i) == sel);
    }
    gfx_draw_text(218, 28, (scroll_top > 0) ? "^" : " ", COLOR_CYAN, COLOR_CARD_BG, false);
    gfx_draw_text(218, 118, (scroll_top + page_size < num_tones) ? "v" : " ", COLOR_CYAN, COLOR_CARD_BG, false);
}

static void audition_channel_sound(int ch, int sel) {
    if (ch == 0) {
        sel %= NUM_TONES_CH1;
        u8 duty = ch1_duty[sel];
        u8 env = ch1_env[sel];
        REG_SOUND1CNT_L = ch1_sweep[sel];
        REG_SOUND1CNT_H = (u16)(((duty & 3) << 6) | (15 << 12) | (env << 8));
        REG_SOUND1CNT_X = (u16)(0x8000 | (midi_to_gba_freq[60] & 0x07FF));
    } else if (ch == 1) {
        sel %= NUM_TONES_CH2;
        u8 duty = ch2_duty[sel];
        u8 env = ch2_env[sel];
        REG_SOUND2CNT_L = (u16)(((duty & 3) << 6) | (15 << 12) | (env << 8));
        REG_SOUND2CNT_H = (u16)(0x8000 | (midi_to_gba_freq[64] & 0x07FF));
    } else if (ch == 2) {
        sel %= NUM_TONES_CH3;
        apply_wave_ram(sel);
        REG_SOUND3CNT_L = 0x0080;
        REG_SOUND3CNT_H = 0x2000;
        REG_SOUND3CNT_X = (u16)(0x8000 | (midi_to_gba_freq[48] & 0x07FF));
    } else if (ch == 3) {
        sel %= NUM_TONES_CH4;
        if (sel == 4) {
            /* Pitched 7-bit bass: note 36 */
            REG_SOUND4CNT_L = (15 << 12) | (3 << 8);
            REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | 3 | (5 << 4));
        } else if (sel == 5) {
            /* Pitched metallic lead: note 60 */
            REG_SOUND4CNT_L = (15 << 12) | (2 << 8);
            REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | 2 | (1 << 4));
        } else if (sel == 6) {
            /* Laser zap */
            REG_SOUND4CNT_L = (15 << 12) | (1 << 8);
            REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | 1 | (2 << 4));
        } else if (sel == 7) {
            /* Lo-Fi brush */
            REG_SOUND4CNT_L = (10 << 12) | (1 << 8);
            REG_SOUND4CNT_H = 0x8000 | 0 | (1 << 4);
        } else if (sel == 8) {
            /* Power Industrial Techno Kick/Snare */
            REG_SOUND4CNT_L = (15 << 12) | (1 << 8);
            REG_SOUND4CNT_H = 0x8000 | 1 | (7 << 4);
        } else if (sel == 9) {
            /* Arcade Alien SFX & Explosions */
            REG_SOUND4CNT_L = (15 << 12) | (6 << 8);
            REG_SOUND4CNT_H = 0x8000 | (1 << 3) | 0 | (4 << 4);
        } else if (sel == 10) {
            /* Dubstep Metallic Snare & Hat */
            REG_SOUND4CNT_L = (15 << 12) | (2 << 8);
            REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | 1 | (2 << 4));
        } else if (sel == 11) {
            /* Arcade Explosion & Sci-Fi SFX */
            REG_SOUND4CNT_L = (15 << 12) | (7 << 8);
            REG_SOUND4CNT_H = 0x8000 | 3 | (4 << 4);
        } else if (sel == 0) {
            /* 808/909 snare */
            REG_SOUND4CNT_L = (15 << 12) | (2 << 8);
            REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | 2 | (3 << 4));
        } else if (sel == 1) {
            /* Acoustic Snare */
            REG_SOUND4CNT_L = (15 << 12) | (3 << 8);
            REG_SOUND4CNT_H = 0x8000 | 2 | (3 << 4);
        } else if (sel == 2) {
            /* 8-bit DMG Snare */
            REG_SOUND4CNT_L = (15 << 12) | (2 << 8);
            REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | 1 | (2 << 4));
        } else {
            /* Rock Snare */
            REG_SOUND4CNT_L = (15 << 12) | (5 << 8);
            REG_SOUND4CNT_H = (u16)(0x8000 | (1 << 3) | 3 | (2 << 4));
        }
    } else if (ch == 4) {
        sel %= NUM_TONES_DS;
        if (sel == 0) {
            sound_play_directsound_a(ds_snare_pcm, sizeof(ds_snare_pcm), pan_values[g_ch_pan[4] % 3], g_ch_vol[4]);
            g_ds_frames_left[0] = (int)(((u64)sizeof(ds_snare_pcm) * 1024ULL * 60 + 16777215ULL) / 16777216ULL) + 1;
        } else {
            sound_play_directsound_a_note(ds_melodic_samples[sel], ds_melodic_lengths[sel], ds_note_timer_reload[60], pan_values[g_ch_pan[4] % 3], g_ch_vol[4]);
            u32 cycles = 65536 - ds_note_timer_reload[60];
            g_ds_frames_left[0] = (int)(((u64)ds_melodic_lengths[sel] * cycles * 60 + 16777215ULL) / 16777216ULL) + 1;
        }
    } else if (ch == 5) {
        sel %= NUM_TONES_DS;
        if (sel == 0) {
            sound_play_directsound_b(ds_snare_pcm, sizeof(ds_snare_pcm), pan_values[g_ch_pan[5] % 3], g_ch_vol[5]);
            g_ds_frames_left[1] = (int)(((u64)sizeof(ds_snare_pcm) * 1024ULL * 60 + 16777215ULL) / 16777216ULL) + 1;
        } else {
            sound_play_directsound_b_note(ds_melodic_samples[sel], ds_melodic_lengths[sel], ds_note_timer_reload[60], pan_values[g_ch_pan[5] % 3], g_ch_vol[5]);
            u32 cycles = 65536 - ds_note_timer_reload[60];
            g_ds_frames_left[1] = (int)(((u64)ds_melodic_lengths[sel] * cycles * 60 + 16777215ULL) / 16777216ULL) + 1;
        }
    }
}

static int open_sound_select_modal(int ch, int current_sound) {
    g_modal_depth++;
    int num_tones = get_num_tones(ch);
    int sel = current_sound % num_tones;
    int scroll_top = 0;
    const int page_size = 10;

    if (sel >= page_size) {
        scroll_top = sel - page_size + 1;
        if (scroll_top > num_tones - page_size) {
            scroll_top = num_tones - page_size;
        }
    }

    /* Draw static frame ONCE */
    gfx_draw_rect(10, 13, 220, 136, COLOR_CARD_BG);
    gfx_draw_rect(10, 13, 220, 1, ch_colors[ch]);
    gfx_draw_rect(10, 148, 220, 1, ch_colors[ch]);
    gfx_draw_rect(10, 13, 1, 136, ch_colors[ch]);
    gfx_draw_rect(229, 13, 1, 136, ch_colors[ch]);

    gfx_draw_text_fmt(16, 16, ch_colors[ch], COLOR_CARD_BG, false,
                      "SELECT SOUND - %s (LIVE AUDITION)", ch_names[ch]);
    gfx_draw_rect(14, 25, 212, 1, COLOR_DARK_GRAY);
    gfx_draw_rect(14, 136, 212, 1, COLOR_DARK_GRAY);
    gfx_draw_text(16, 138, "UP/DN: Scroll & Audition | A: Select | B: Back", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);

    draw_sound_select_page(ch, scroll_top, sel, current_sound % num_tones);

    while (1) {
        vsync();
        input_poll();

        /* LIVE BACKGROUND PLAYBACK */
        step_sequencer();

        if (key_hit(KEY_B)) {
            /* Cancel and revert to previous sound */
            g_ch_tone[ch] = current_sound % num_tones;
            if (ch == 2) apply_wave_ram(g_ch_tone[2]);
            g_modal_depth--;
            return current_sound % num_tones;
        }

        if (key_hit(KEY_A)) {
            g_modal_depth--;
            return sel; /* Confirm chosen sound */
        }

        if (key_hit(KEY_UP)) {
            int prev = sel;
            sel = (sel - 1 + num_tones) % num_tones;
            g_ch_tone[ch] = sel;
            audition_channel_sound(ch, sel);

            int prev_scroll = scroll_top;
            if (sel < scroll_top) {
                scroll_top = sel;
            } else if (sel >= scroll_top + page_size) {
                scroll_top = sel - page_size + 1;
            }

            if (scroll_top != prev_scroll) {
                draw_sound_select_page(ch, scroll_top, sel, current_sound % num_tones);
            } else {
                draw_sound_select_row(ch, prev, prev - scroll_top, current_sound % num_tones, false);
                draw_sound_select_row(ch, sel, sel - scroll_top, current_sound % num_tones, true);
            }
        }
        if (key_hit(KEY_DOWN)) {
            int prev = sel;
            sel = (sel + 1) % num_tones;
            g_ch_tone[ch] = sel;
            audition_channel_sound(ch, sel);

            int prev_scroll = scroll_top;
            if (sel < scroll_top) {
                scroll_top = sel;
            } else if (sel >= scroll_top + page_size) {
                scroll_top = sel - page_size + 1;
            }

            if (scroll_top != prev_scroll) {
                draw_sound_select_page(ch, scroll_top, sel, current_sound % num_tones);
            } else {
                draw_sound_select_row(ch, prev, prev - scroll_top, current_sound % num_tones, false);
                draw_sound_select_row(ch, sel, sel - scroll_top, current_sound % num_tones, true);
            }
        }
    }
}

/* --- Modal Window 3: All-In-One Channel Inspector / Settings Modal --- */
static void draw_channel_settings_frame(int ch) {
    gfx_draw_rect(8, 12, 224, 136, COLOR_CARD_BG);
    gfx_draw_rect(8, 12, 224, 1, ch_colors[ch]);
    gfx_draw_rect(8, 147, 224, 1, ch_colors[ch]);
    gfx_draw_rect(8, 12, 1, 136, ch_colors[ch]);
    gfx_draw_rect(231, 12, 1, 136, ch_colors[ch]);

    gfx_draw_text_fmt(14, 15, ch_colors[ch], COLOR_CARD_BG, false, "CHANNEL %s INSPECTOR & SETTINGS", ch_names[ch]);
    gfx_draw_rect(12, 24, 216, 1, COLOR_DARK_GRAY);

    /* Hardware Guide Box Frame (y = 93 to 132) */
    gfx_draw_rect(12, 93, 216, 1, COLOR_DARK_GRAY);
    gfx_draw_rect(12, 95, 216, 37, COLOR_HEADER_BG);

    if (ch == 0) {
        gfx_draw_text(16,  97, "CH1: Square 1 Pulse mit Pitch Sweep Hardware.", COLOR_CYAN, COLOR_HEADER_BG, false);
        gfx_draw_text(16, 107, "Optimal fuer: Hauptmelodie, Gesang, schnelle Arps.", COLOR_WHITE, COLOR_HEADER_BG, false);
        gfx_draw_text(16, 117, "Tipp: 50% Pulse klingt voll & warm, 25% retro.", COLOR_LIGHT_GRAY, COLOR_HEADER_BG, false);
    } else if (ch == 1) {
        gfx_draw_text(16,  97, "CH2: Pure Square 2 Melodic Pulse Voice.", COLOR_GOLD, COLOR_HEADER_BG, false);
        gfx_draw_text(16, 107, "Optimal fuer: Harmonien, Zweitstimme, Glocken.", COLOR_WHITE, COLOR_HEADER_BG, false);
        gfx_draw_text(16, 117, "Tipp: Perfekt als harmonische Gegenstimme zu CH1.", COLOR_LIGHT_GRAY, COLOR_HEADER_BG, false);
    } else if (ch == 2) {
        gfx_draw_text(16,  97, "CH3: 32-Sample Wave RAM Custom Synthesizer.", COLOR_EMERALD, COLOR_HEADER_BG, false);
        gfx_draw_text(16, 107, "Optimal fuer: Basslines, Floeten, Orgeln, Leads.", COLOR_WHITE, COLOR_HEADER_BG, false);
        gfx_draw_text(16, 117, "Tipp: Triangle = DMG Bass, Sine = Deep 808 Sub.", COLOR_LIGHT_GRAY, COLOR_HEADER_BG, false);
    } else if (ch == 3) {
        gfx_draw_text(16,  97, "CH4: White Noise PSG Percussion Synth.", ch_colors[3], COLOR_HEADER_BG, false);
        gfx_draw_text(16, 107, "Optimal fuer: Retro Drums, Snare, Hi-Hats, Zaps.", COLOR_WHITE, COLOR_HEADER_BG, false);
        gfx_draw_text(16, 117, "Tipp: 7-Step Noise fuer metallischen 8-Bit Klang.", COLOR_LIGHT_GRAY, COLOR_HEADER_BG, false);
    } else if (ch == 4) {
        gfx_draw_text(16,  97, "DSA: DirectSound A 8-Bit PCM (DMA 1, Timer 0).", COLOR_CORAL, COLOR_HEADER_BG, false);
        gfx_draw_text(16, 107, "Digital Audio: Real Drums, Piano, Bass, Strings.", COLOR_WHITE, COLOR_HEADER_BG, false);
        gfx_draw_text(16, 117, "Tipp: Echtes 8-Bit PCM Sample Playback via FIFO A.", COLOR_LIGHT_GRAY, COLOR_HEADER_BG, false);
    } else {
        gfx_draw_text(16,  97, "DSB: DirectSound B 8-Bit PCM (DMA 2, Timer 1).", ch_colors[5], COLOR_HEADER_BG, false);
        gfx_draw_text(16, 107, "Zweiter PCM Kanal: Brass, Rock Guitar, Flute, Synth.", COLOR_WHITE, COLOR_HEADER_BG, false);
        gfx_draw_text(16, 117, "Tipp: Laeuft parallel zu allen anderen 5 Kanaelen!", COLOR_LIGHT_GRAY, COLOR_HEADER_BG, false);
    }

    gfx_draw_rect(12, 134, 216, 1, COLOR_DARK_GRAY);
    gfx_draw_text(16, 137, "UP/DN: Nav | L/R: Wert | A: Menue (Spur/Klang) | B: Back", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
}

static void draw_channel_settings_row(int ch, int r, bool is_sel) {
    u16 bg_col = is_sel ? COLOR_CARD_BORDER : COLOR_CARD_BG;
    int y = 27 + r * 13;
    gfx_draw_rect(12, y, 216, 12, bg_col);

    if (r == 0) {
        gfx_draw_text_fmt(16, y + 2, is_sel ? COLOR_CYAN : COLOR_WHITE, bg_col, false,
                          "%s [SPUREN] Zuweisung (A: Menue oeffnen)", is_sel ? ">" : " ");
    } else if (r == 1) {
        int num_tones = get_num_tones(ch);
        int tone_idx = g_ch_tone[ch] % num_tones;
        const char* desc = "";
        if (ch == 0) desc = ch1_tones[tone_idx];
        else if (ch == 1) desc = ch2_tones[tone_idx];
        else if (ch == 2) desc = ch3_tones[tone_idx];
        else if (ch == 3) desc = ch4_tones[tone_idx];
        else desc = ds_tones[tone_idx];

        gfx_draw_text_fmt(16, y + 2, is_sel ? COLOR_GOLD : COLOR_WHITE, bg_col, false,
                          "%s [SOUND ] Klang: < %s > (A: Liste)", is_sel ? ">" : " ", desc);
    } else if (r == 2) {
        gfx_draw_text_fmt(16, y + 2, is_sel ? COLOR_GOLD : COLOR_WHITE, bg_col, false,
                          "%s [OKTAVE] Transpose: < [Oct:%+d] >", is_sel ? ">" : " ", g_ch_octave[ch]);
    } else if (r == 3) {
        gfx_draw_text_fmt(16, y + 2, is_sel ? COLOR_GOLD : COLOR_WHITE, bg_col, false,
                          "%s [VOLUME] Pegel:     < %3d%% >", is_sel ? ">" : " ", g_ch_vol[ch]);
    } else if (r == 4) {
        gfx_draw_text_fmt(16, y + 2, is_sel ? COLOR_GOLD : COLOR_WHITE, bg_col, false,
                          "%s [PAN   ] Stereo:    < %s >", is_sel ? ">" : " ", pan_names[g_ch_pan[ch] % 3]);
    }
}

static void open_channel_settings_modal(int ch) {
    g_modal_depth++;
    int sel_row = 0; /* 0=Tracks, 1=Instrument, 2=Octave, 3=Volume, 4=Pan */
    const int total_rows = 5;

    draw_channel_settings_frame(ch);
    for (int i = 0; i < total_rows; i++) {
        draw_channel_settings_row(ch, i, i == sel_row);
    }

    while (1) {
        vsync();
        input_poll();

        /* LIVE BACKGROUND PLAYBACK */
        step_sequencer();

        if (key_hit(KEY_B)) {
            g_modal_depth--;
            break; /* Exit modal */
        }

        if (key_hit(KEY_UP)) {
            int prev = sel_row;
            sel_row = (sel_row - 1 + total_rows) % total_rows;
            draw_channel_settings_row(ch, prev, false);
            draw_channel_settings_row(ch, sel_row, true);
        }
        if (key_hit(KEY_DOWN)) {
            int prev = sel_row;
            sel_row = (sel_row + 1) % total_rows;
            draw_channel_settings_row(ch, prev, false);
            draw_channel_settings_row(ch, sel_row, true);
        }

        /* Action or sub-modal */
        if (key_hit(KEY_A)) {
            if (sel_row == 0) {
                g_ch_track_mask[ch] = open_track_select_modal(ch, g_ch_track_mask[ch], &g_midi_songs[g_cur_song_idx]);
                draw_channel_settings_frame(ch);
                for (int i = 0; i < total_rows; i++) draw_channel_settings_row(ch, i, i == sel_row);
            } else if (sel_row == 1) {
                g_ch_tone[ch] = open_sound_select_modal(ch, g_ch_tone[ch]);
                if (ch == 2) apply_wave_ram(g_ch_tone[2]);
                draw_channel_settings_frame(ch);
                for (int i = 0; i < total_rows; i++) draw_channel_settings_row(ch, i, i == sel_row);
            }
        }

        /* Adjust parameters with LEFT / RIGHT */
        if (key_hit(KEY_LEFT)) {
            if (sel_row == 1) {
                /* Instrument Preset */
                int num_tones = get_num_tones(ch);
                g_ch_tone[ch] = (g_ch_tone[ch] - 1 + num_tones) % num_tones;
                if (ch == 2) apply_wave_ram(g_ch_tone[2]);
                draw_channel_settings_row(ch, sel_row, true);
            } else if (sel_row == 2) {
                /* Octave Shift */
                if (g_ch_octave[ch] > -2) {
                    g_ch_octave[ch]--;
                    retune_active_channel_note(ch);
                }
                draw_channel_settings_row(ch, sel_row, true);
            } else if (sel_row == 3) {
                /* Volume in 5% steps */
                if (g_ch_vol[ch] >= 5) g_ch_vol[ch] -= 5;
                apply_channel_volume(ch);
                int max_lvl = (24 * g_ch_vol[ch] + 50) / 100;
                if (s_vu_level[ch] > max_lvl) s_vu_level[ch] = (u8)max_lvl;
                draw_channel_settings_row(ch, sel_row, true);
            } else if (sel_row == 4) {
                /* Panning */
                g_ch_pan[ch] = (g_ch_pan[ch] - 1 + 3) % 3;
                sound_set_pan(ch + 1, pan_values[g_ch_pan[ch] % 3]);
                draw_channel_settings_row(ch, sel_row, true);
            }
        }
        if (key_hit(KEY_RIGHT)) {
            if (sel_row == 1) {
                /* Instrument Preset */
                int num_tones = get_num_tones(ch);
                g_ch_tone[ch] = (g_ch_tone[ch] + 1) % num_tones;
                if (ch == 2) apply_wave_ram(g_ch_tone[2]);
                draw_channel_settings_row(ch, sel_row, true);
            } else if (sel_row == 2) {
                /* Octave Shift */
                if (g_ch_octave[ch] < 2) {
                    g_ch_octave[ch]++;
                    retune_active_channel_note(ch);
                }
                draw_channel_settings_row(ch, sel_row, true);
            } else if (sel_row == 3) {
                /* Volume in 5% steps */
                if (g_ch_vol[ch] <= 95) g_ch_vol[ch] += 5;
                apply_channel_volume(ch);
                int max_lvl = (24 * g_ch_vol[ch] + 50) / 100;
                if (s_vu_level[ch] > max_lvl) s_vu_level[ch] = (u8)max_lvl;
                draw_channel_settings_row(ch, sel_row, true);
            } else if (sel_row == 4) {
                /* Panning */
                g_ch_pan[ch] = (g_ch_pan[ch] + 1) % 3;
                sound_set_pan(ch + 1, pan_values[g_ch_pan[ch] % 3]);
                draw_channel_settings_row(ch, sel_row, true);
            }
        }
    }
}

/* --- Modal Window 5: Song Multi-Preset Manager (Loaded via R on Song Row) --- */
static void draw_preset_card(int song_idx, int slot, bool is_sel) {
    int y = 33 + slot * 26;
    u16 bg_col = is_sel ? COLOR_CARD_BORDER : COLOR_CARD_BG;
    u16 outline_col = is_sel ? COLOR_CYAN : COLOR_DARK_GRAY;

    gfx_draw_rect(12, y, 216, 24, bg_col);
    gfx_draw_rect_outline(12, y, 216, 24, outline_col);

    const SongPreset* p = &g_song_presets[song_idx][slot];
    const char* slot_title = (slot == 0) ? "SLOT 1 [AUTO-LOAD ON TRACK START]" :
                             ((slot == 1) ? "SLOT 2 [CUSTOM PRESET]" : "SLOT 3 [CUSTOM PRESET]");

    u16 title_col = is_sel ? COLOR_GOLD : COLOR_WHITE;
    gfx_draw_text_fmt(16, y + 2, title_col, bg_col, false, "%s %s", is_sel ? ">" : " ", slot_title);

    if (p->is_valid) {
        int act_ch = 0;
        for (int c = 0; c < 6; c++) {
            if (p->track_mask[c] != 0 && !p->mute[c]) act_ch++;
        }
        gfx_draw_text_fmt(24, y + 12, COLOR_EMERALD, bg_col, false,
                          "[SAVED] %d Active CHs | Oct:%+d/%+d/%+d..",
                          act_ch, p->octave[0], p->octave[1], p->octave[2]);
    } else {
        gfx_draw_text(24, y + 12, "-- EMPTY -- Press START to save current mix",
                      is_sel ? COLOR_LIGHT_GRAY : COLOR_DARK_GRAY, bg_col, false);
    }
}

static void open_song_preset_modal(int song_idx) {
    g_modal_depth++;
    int sel = 0;
    const char* feedback_msg = "";
    u16 feedback_col = COLOR_EMERALD;

    /* Draw static outer frame */
    gfx_draw_rect(8, 16, 224, 130, COLOR_CARD_BG);
    gfx_draw_rect(8, 16, 224, 1, COLOR_CYAN);
    gfx_draw_rect(8, 145, 224, 1, COLOR_CYAN);
    gfx_draw_rect(8, 16, 1, 130, COLOR_CYAN);
    gfx_draw_rect(231, 16, 1, 130, COLOR_CYAN);

    /* Header */
    gfx_draw_text(12, 19, "SONG PRESETS", COLOR_GOLD, COLOR_CARD_BG, false);
    gfx_draw_text_clipped(96, 19, 226, g_midi_songs[song_idx].title, COLOR_CYAN, COLOR_CARD_BG, false);
    gfx_draw_line_h(10, 28, 220, COLOR_DARK_GRAY);

    /* Draw initial cards */
    for (int s = 0; s < MIDI_PRESETS_PER_SONG; s++) {
        draw_preset_card(song_idx, s, s == sel);
    }

    /* Footer instructions */
    gfx_draw_line_h(10, 114, 220, COLOR_DARK_GRAY);
    gfx_draw_text(12, 117, "A:Load  STR:Save  SEL:Clear  B:Back", COLOR_GOLD, COLOR_CARD_BG, false);

    while (1) {
        vsync();
        input_poll();
        step_sequencer();

        if (key_hit(KEY_B)) {
            g_modal_depth--;
            break;
        }

        if (key_hit(KEY_UP)) {
            int prev = sel;
            sel = (sel - 1 + MIDI_PRESETS_PER_SONG) % MIDI_PRESETS_PER_SONG;
            draw_preset_card(song_idx, prev, false);
            draw_preset_card(song_idx, sel, true);
        }
        if (key_hit(KEY_DOWN)) {
            int prev = sel;
            sel = (sel + 1) % MIDI_PRESETS_PER_SONG;
            draw_preset_card(song_idx, prev, false);
            draw_preset_card(song_idx, sel, true);
        }

        if (key_hit(KEY_A)) {
            if (g_song_presets[song_idx][sel].is_valid) {
                apply_preset(song_idx, sel);
                feedback_msg = "PRESET LOADED INTO LIVE MIXER!";
                feedback_col = COLOR_EMERALD;
            } else {
                feedback_msg = "SLOT EMPTY! PRESS START TO SAVE CURRENT MIX";
                feedback_col = COLOR_AMBER;
            }
            gfx_draw_rect(12, 127, 216, 8, COLOR_CARD_BG);
            gfx_draw_text_clipped(12, 127, 228, feedback_msg, feedback_col, COLOR_CARD_BG, false);
        }

        if (key_hit(KEY_START)) {
            save_current_to_preset(song_idx, sel);
            feedback_msg = (sel == 0) ? "SAVED TO SLOT 1 (DEFAULT ON SONG START)!" : "SAVED CURRENT MIX TO THIS SLOT!";
            feedback_col = COLOR_EMERALD;
            draw_preset_card(song_idx, sel, true);
            gfx_draw_rect(12, 127, 216, 8, COLOR_CARD_BG);
            gfx_draw_text_clipped(12, 127, 228, feedback_msg, feedback_col, COLOR_CARD_BG, false);
        }

        if (key_hit(KEY_SELECT)) {
            if (g_song_presets[song_idx][sel].is_valid) {
                clear_preset(song_idx, sel);
                feedback_msg = "CLEARED SLOT FROM CARTRIDGE SRAM!";
                feedback_col = COLOR_RED;
                draw_preset_card(song_idx, sel, true);
            } else {
                feedback_msg = "SLOT IS ALREADY EMPTY";
                feedback_col = COLOR_LIGHT_GRAY;
            }
            gfx_draw_rect(12, 127, 216, 8, COLOR_CARD_BG);
            gfx_draw_text_clipped(12, 127, 228, feedback_msg, feedback_col, COLOR_CARD_BG, false);
        }
    }
}

static void draw_transport_status(bool is_playing);

static void switch_to_song(int new_song_idx) {
    g_cur_song_idx = new_song_idx % MIDI_SONG_COUNT;
    silence_hardware();
    for (int c = 0; c < 6; c++) {
        g_ch_cur_note[c] = 0;
        g_ch_vis_note[c] = 0;
        g_ch_vis_hold[c] = 0;
        g_ch_solo[c] = false; /* Clear solos on song switch */
    }
    g_cur_frame = 0;
    g_event_idx = 0;
    g_is_playing = true;
    load_song_active_configuration(g_cur_song_idx);
    reset_waterfall();
    draw_transport_status(g_is_playing);
}

static void draw_diskette_icon(int x, int y, u16 fg_col, u16 bg_col) {
    gfx_draw_rect(x, y, 7, 7, bg_col);
    gfx_draw_line_h(x, y, 6, fg_col);
    gfx_draw_pixel(x, y + 1, fg_col);
    gfx_draw_pixel(x + 1, y + 1, fg_col);
    gfx_draw_pixel(x + 4, y + 1, fg_col);
    gfx_draw_pixel(x + 5, y + 1, fg_col);
    gfx_draw_pixel(x + 6, y + 1, fg_col);
    gfx_draw_pixel(x, y + 2, fg_col);
    gfx_draw_pixel(x + 1, y + 2, fg_col);
    gfx_draw_pixel(x + 4, y + 2, fg_col);
    gfx_draw_pixel(x + 5, y + 2, fg_col);
    gfx_draw_pixel(x + 6, y + 2, fg_col);
    gfx_draw_line_h(x, y + 3, 7, fg_col);
    gfx_draw_pixel(x, y + 4, fg_col);
    gfx_draw_line_h(x + 2, y + 4, 4, fg_col);
    gfx_draw_pixel(x + 6, y + 4, fg_col);
    gfx_draw_pixel(x, y + 5, fg_col);
    gfx_draw_line_h(x + 2, y + 5, 4, fg_col);
    gfx_draw_pixel(x + 6, y + 5, fg_col);
    gfx_draw_line_h(x, y + 6, 7, fg_col);
}

static void draw_song_row(bool is_sel) {
    u16 bg_col = is_sel ? COLOR_CARD_BORDER : COLOR_CARD_BG;
    u16 fg_col = is_sel ? COLOR_GOLD : COLOR_CYAN;

    gfx_draw_rect(5, 17, 230, 8, bg_col);
    gfx_draw_text(6, 17, is_sel ? ">" : " ", fg_col, bg_col, false);

    bool has_cst = g_song_presets[g_cur_song_idx][0].is_valid;
    if (has_cst) {
        draw_diskette_icon(14, 17, COLOR_EMERALD, bg_col);
    } else {
        gfx_draw_rect(14, 17, 7, 7, bg_col);
    }

    gfx_draw_text_fmt(23, 17, fg_col, bg_col, false, "[%02d/%02d]",
                      g_cur_song_idx + 1,
                      MIDI_SONG_COUNT);

    gfx_draw_text_clipped(67, 17, 199, g_midi_songs[g_cur_song_idx].title, fg_col, bg_col, false);

    gfx_draw_text_fmt(202, 17, is_sel ? COLOR_GOLD : COLOR_LIGHT_GRAY, bg_col, false, "%3dBPM", g_midi_songs[g_cur_song_idx].bpm);
}

static void draw_transport_status(bool is_playing) {
    gfx_draw_rect(88, 27, 29, 8, COLOR_CARD_BG);
    if (is_playing) {
        /* Light blue / Cyan Play button triangle ▶ */
        gfx_draw_line_v(89, 28, 6, COLOR_CYAN);
        gfx_draw_line_v(90, 29, 4, COLOR_CYAN);
        gfx_draw_line_v(91, 30, 2, COLOR_CYAN);
        gfx_draw_text(95, 27, "PLAY", COLOR_CYAN, COLOR_CARD_BG, false);
    } else {
        /* Light blue / Cyan Stop square ■ */
        gfx_draw_rect(89, 28, 5, 5, COLOR_CYAN);
        gfx_draw_text(96, 27, "STOP", COLOR_CYAN, COLOR_CARD_BG, false);
    }
}

static void draw_mixer_row(int c, bool is_sel) {
    int y = 47 + c * 8;
    u16 bg_col = is_sel ? COLOR_CARD_BORDER : COLOR_CARD_BG;

    gfx_draw_rect(5, y, 230, 8, bg_col);
    if (is_sel) {
        gfx_draw_text(6, y + 1, ">", COLOR_CYAN, bg_col, false);
    }

    /* 1. Channel Tag in unique color */
    gfx_draw_text(13, y + 1, ch_names[c], ch_colors[c], bg_col, false);

    /* 2. Compact Live / Mute Icon (Green Play Triangle ▶ / Red Stop Square ■ / Golden S) */
    draw_playback_badge(32, y + 1, c, bg_col);

    /* 3. Octave Shift Indicator */
    s8 oct = g_ch_octave[c];
    u16 oct_col = (oct != 0) ? COLOR_GOLD : (is_sel ? COLOR_WHITE : COLOR_LIGHT_GRAY);
    gfx_draw_text_fmt(41, y + 1, oct_col, bg_col, false, "[%+d]", oct);

    /* 4. Assigned Multi-Track Description */
    u16 mask = g_ch_track_mask[c];
    int active_count = 0;
    int first_trk = -1;
    for (int t = 0; t < g_midi_songs[g_cur_song_idx].track_count; t++) {
        if (mask & (1 << t)) {
            active_count++;
            if (first_trk < 0) first_trk = t;
        }
    }

    u16 trk_col = COLOR_DARK_GRAY;
    if (active_count == 0) {
        gfx_draw_text(68, y + 1, "[Unassigned]", COLOR_DARK_GRAY, bg_col, false);
    } else if (active_count == 1) {
        trk_col = g_ch_mute[c] ? COLOR_DARK_GRAY : (is_sel ? COLOR_GOLD : COLOR_CYAN);
        gfx_draw_text_clipped(68, y + 1, 148, g_midi_songs[g_cur_song_idx].tracks[first_trk].name, trk_col, bg_col, false);
    } else {
        trk_col = g_ch_mute[c] ? COLOR_DARK_GRAY : (is_sel ? COLOR_GOLD : COLOR_EMERALD);
        gfx_draw_text_fmt(68, y + 1, trk_col, bg_col, false, "[%d Tracks]", active_count);
    }

    /* 5. Instrument / Tone Box */
    int num_tones = get_num_tones(c);
    int tone_idx = g_ch_tone[c] % num_tones;
    const char* tone_desc = "";
    if (c == 0) tone_desc = ch1_tones[tone_idx];
    else if (c == 1) tone_desc = ch2_tones[tone_idx];
    else if (c == 2) tone_desc = ch3_tones[tone_idx];
    else if (c == 3) tone_desc = ch4_tones[tone_idx];
    else tone_desc = ds_tones[tone_idx];

    gfx_draw_text_clipped(152, y + 1, 234, tone_desc, is_sel ? COLOR_WHITE : COLOR_LIGHT_GRAY, bg_col, false);
}

static void draw_mixer_full_screen(int cursor_row) {
    gfx_clear(COLOR_NAVY);
    gfx_draw_header("MIDI SEQUENCER", "6-CH MIXER & PIANO");
    gfx_draw_footer("L:Play/Rst | R:Save | A:Edit | B:Back");

    /* Card 1: Song Header & Progress Container (x = 4, y = 16, w = 232, h = 20) */
    gfx_draw_rect(4, 16, 232, 20, COLOR_CARD_BG);
    gfx_draw_rect_outline(4, 16, 232, 20, COLOR_CARD_BORDER);
    draw_song_row(cursor_row == 0);

    /* Progress bar outline container and Transport Status Icon */
    gfx_draw_rect_outline(118, 27, 114, 6, COLOR_DARK_GRAY);
    draw_transport_status(g_is_playing);

    /* Card 2: 6-Channel Mixer Console Container (x = 4, y = 37, w = 232, h = 60) */
    gfx_draw_rect(4, 37, 232, 60, COLOR_CARD_BG);
    gfx_draw_rect_outline(4, 37, 232, 60, COLOR_CARD_BORDER);

    /* Card 2 Header Bar (y = 37..46) - Column header "CH" directly aligned at x = 13 */
    gfx_draw_rect(5, 38, 230, 8, COLOR_HEADER_BG);
    gfx_draw_line_h(4, 46, 232, COLOR_CARD_BORDER);
    gfx_draw_text(13, 39, "CH", COLOR_CYAN_GLOW, COLOR_HEADER_BG, false);
    gfx_draw_text(41, 39, "OCT", COLOR_LIGHT_GRAY, COLOR_HEADER_BG, false);
    gfx_draw_text(68, 39, "ASSIGNED SPUR", COLOR_LIGHT_GRAY, COLOR_HEADER_BG, false);
    gfx_draw_text(152, 39, "KLANG / PRESET", COLOR_LIGHT_GRAY, COLOR_HEADER_BG, false);

    /* Draw all 6 mixer rows */
    for (int c = 0; c < 6; c++) {
        draw_mixer_row(c, cursor_row == (c + 1));
    }

    /* Reset Piano Visualizer & VU meters */
    reset_piano_visualizer_state();
}

/* --- Main MIDI Sequencer Execution --- */
void run_midi_player(void) {
    sound_stop_all();
    sound_init();

    /* Load persisted configuration from SRAM / EZ-Flash */
    bool restored = midi_load_config();
    load_song_active_configuration(g_cur_song_idx);

    int cursor_row = 1; /* Default to CH1 */
    g_is_playing = true;
    g_cur_frame = 0;
    g_event_idx = 0;

    for (int c = 0; c < 6; c++) {
        g_ch_cur_note[c] = 0;
        sound_set_pan(c + 1, pan_values[g_ch_pan[c] % 3]);
        apply_channel_volume(c);
    }
    apply_wave_ram(g_ch_tone[2]);

    int save_notify_frames = restored ? 60 : 0;
    const char* notify_msg = restored ? "LOADED CONFIG FROM EZ-FLASH SRAM!" : "";

    draw_mixer_full_screen(cursor_row);
    reset_waterfall();

    int prev_sec = -1;
    int prev_bar_w = -1;

    static int s_l_hold = 0;
    static bool s_l_restarted = false;
    static int s_start_hold = 0;
    static bool s_start_solo_triggered = false;

    while (1) {
        vsync();
        input_poll();

        if (key_hit(KEY_B)) {
            sound_stop_all();
            break;
        }

        /* 1. Play / Pause (tap) or Restart Track (hold > 25 frames) with L Shoulder Button */
        if (key_down(KEY_L)) {
            s_l_hold++;
            if (s_l_hold >= 25 && !s_l_restarted) {
                /* Held down for ~0.4s: RESTART TRACK FROM 0:00 without pausing! */
                s_l_restarted = true;
                silence_hardware();
                for (int c = 0; c < 6; c++) {
                    g_ch_cur_note[c] = 0;
                    g_ch_vis_note[c] = 0;
                    g_ch_vis_hold[c] = 0;
                }
                g_cur_frame = 0;
                g_event_idx = 0;
                g_is_playing = true;
                reset_waterfall();
                draw_transport_status(g_is_playing);
                prev_sec = -1;
                /* Reset progress bar cleanly */
                gfx_draw_rect(119, 28, 112, 4, COLOR_CARD_BG);
                prev_bar_w = 0;
            }
        } else {
            /* L released */
            if (s_l_hold > 0 && !s_l_restarted) {
                /* Short tap (< 25 frames): toggle play / pause */
                g_is_playing = !g_is_playing;
                if (!g_is_playing) {
                    silence_hardware();
                    for (int c = 0; c < 6; c++) g_ch_cur_note[c] = 0;
                }
                draw_transport_status(g_is_playing);
                prev_sec = -1;
            }
            s_l_hold = 0;
            s_l_restarted = false;
        }

        /* 2. Cartridge SRAM / Preset Management with R Shoulder Button */
        if (key_hit(KEY_R)) {
            if (cursor_row == 0) {
                open_song_preset_modal(g_cur_song_idx);
                draw_mixer_full_screen(cursor_row);
                catchup_waterfall();
                prev_sec = -1;
                prev_bar_w = -1;
            } else if (cursor_row >= 1 && cursor_row <= 6) {
                save_current_to_preset(g_cur_song_idx, 0);
                save_notify_frames = 90;
                notify_msg = "SAVED TO SLOT 1 (AUTO-LOAD) & SRAM!";
                /* Redraw song row immediately so diskette icon appears right away */
                draw_song_row(cursor_row == 0);
            }
        }

        /* 3. Ultra-Fast Dirty-Row Navigation with UP / DOWN */
        if (key_hit(KEY_UP)) {
            int prev_row = cursor_row;
            cursor_row = (cursor_row - 1 + 7) % 7;
            if (prev_row == 0) draw_song_row(false);
            else draw_mixer_row(prev_row - 1, false);

            if (cursor_row == 0) draw_song_row(true);
            else draw_mixer_row(cursor_row - 1, true);
        }
        if (key_hit(KEY_DOWN)) {
            int prev_row = cursor_row;
            cursor_row = (cursor_row + 1) % 7;
            if (prev_row == 0) draw_song_row(false);
            else draw_mixer_row(prev_row - 1, false);

            if (cursor_row == 0) draw_song_row(true);
            else draw_mixer_row(cursor_row - 1, true);
        }

        /* 4. Action with A Button:
           - On Row 0: Open Song Selection Modal
           - On Row 1..6: Open All-In-One Channel Inspector / Settings Modal! */
        if (key_hit(KEY_A)) {
            if (cursor_row == 0) {
                int chosen = open_song_select_modal(g_cur_song_idx);
                if (chosen != g_cur_song_idx) {
                    switch_to_song(chosen);
                }
            } else if (cursor_row >= 1 && cursor_row <= 6) {
                int ch = cursor_row - 1;
                open_channel_settings_modal(ch);
            }

            /* Full clean redraw after modal closes */
            draw_mixer_full_screen(cursor_row);
            catchup_waterfall();
            prev_sec = -1;
            prev_bar_w = -1;
        }

        /* 5. SELECT Button: Quick Cycle Instrument Tone for highlighted channel */
        if (key_hit(KEY_SELECT)) {
            if (cursor_row >= 1 && cursor_row <= 6) {
                int ch = cursor_row - 1;
                int num_tones = get_num_tones(ch);
                g_ch_tone[ch] = (g_ch_tone[ch] + 1) % num_tones;
                if (ch == 2) apply_wave_ram(g_ch_tone[2]);
                draw_mixer_row(ch, true);
            }
        }

        /* 6. START Button:
           - Tap (< 20 frames): Toggle Mute / Live for highlighted channel
           - Hold (>= 20 frames): Toggle SOLO on highlighted channel! */
        if (cursor_row >= 1 && cursor_row <= 6) {
            int ch = cursor_row - 1;
            if (key_down(KEY_START)) {
                s_start_hold++;
                if (s_start_hold >= 20 && !s_start_solo_triggered) {
                    s_start_solo_triggered = true;
                    g_ch_solo[ch] = !g_ch_solo[ch];
                    if (g_ch_solo[ch]) {
                        g_ch_mute[ch] = false; /* Unmute when entering solo */
                    }
                    /* Silence channels that are not audible in solo mode */
                    for (int c = 0; c < 6; c++) {
                        if (!is_channel_audible(c)) {
                            silence_channel(c);
                            g_ch_cur_note[c] = 0;
                            g_ch_vis_note[c] = 0;
                        }
                    }
                    for (int c = 0; c < 6; c++) {
                        draw_mixer_row(c, cursor_row == (c + 1));
                    }
                }
            } else {
                if (s_start_hold > 0 && !s_start_solo_triggered) {
                    /* Short tap: toggle mute */
                    g_ch_mute[ch] = !g_ch_mute[ch];
                    if (g_ch_mute[ch]) {
                        g_ch_solo[ch] = false;
                        silence_channel(ch);
                        g_ch_cur_note[ch] = 0;
                        g_ch_vis_note[ch] = 0;
                    }
                    for (int c = 0; c < 6; c++) {
                        draw_mixer_row(c, cursor_row == (c + 1));
                    }
                }
                s_start_hold = 0;
                s_start_solo_triggered = false;
            }
        } else {
            s_start_hold = 0;
            s_start_solo_triggered = false;
        }

        /* 7. LEFT / RIGHT Button:
           - On Row 0: Previous / Next Song (Instant Flicker-Free Redraw!)
           - On Row 1..6: Decrement / Increment Octave Shift (-2 to +2)! */
        if (key_hit(KEY_LEFT)) {
            if (cursor_row == 0) {
                int new_song = (g_cur_song_idx - 1 + MIDI_SONG_COUNT) % MIDI_SONG_COUNT;
                switch_to_song(new_song);
                draw_song_row(true);
                for (int c = 0; c < 6; c++) draw_mixer_row(c, false);
                gfx_draw_rect(119, 28, 112, 4, COLOR_CARD_BG);
                prev_sec = -1;
                prev_bar_w = -1;
            } else if (cursor_row >= 1 && cursor_row <= 6) {
                int ch = cursor_row - 1;
                if (g_ch_octave[ch] > -2) {
                    g_ch_octave[ch]--;
                    draw_mixer_row(ch, true);
                    retune_active_channel_note(ch);
                }
            }
        }
        if (key_hit(KEY_RIGHT)) {
            if (cursor_row == 0) {
                int new_song = (g_cur_song_idx + 1) % MIDI_SONG_COUNT;
                switch_to_song(new_song);
                draw_song_row(true);
                for (int c = 0; c < 6; c++) draw_mixer_row(c, false);
                gfx_draw_rect(119, 28, 112, 4, COLOR_CARD_BG);
                prev_sec = -1;
                prev_bar_w = -1;
            } else if (cursor_row >= 1 && cursor_row <= 6) {
                int ch = cursor_row - 1;
                if (g_ch_octave[ch] < 2) {
                    g_ch_octave[ch]++;
                    draw_mixer_row(ch, true);
                    retune_active_channel_note(ch);
                }
            }
        }

        /* 8. Step Sequencer Playback */
        step_sequencer();

        /* 9. Live Waterfall, Piano Keyboard & 6-Channel Activity VU Meters */
        update_piano_visualizer();

        /* 10. Live Progress Bar and Timer (Flicker-Free Incremental Updates) */
        const SongInfo* song = &g_midi_songs[g_cur_song_idx];
        int cur_sec = g_cur_frame / 60;
        int tot_sec = song->total_frames / 60;
        if (cur_sec != prev_sec) {
            prev_sec = cur_sec;
            /* Direct render with background color - no broken %-5s formatting! */
            gfx_draw_text_fmt(6, 27, COLOR_CYAN, COLOR_CARD_BG, false,
                              "[%02d:%02d/%02d:%02d]",
                              cur_sec / 60, cur_sec % 60,
                              tot_sec / 60, tot_sec % 60);
        }

        int bar_w = (g_cur_frame * 112) / (song->total_frames ? song->total_frames : 1);
        if (bar_w != prev_bar_w) {
            if (prev_bar_w < 0 || bar_w < prev_bar_w) {
                /* Full redraw on track rewind / change */
                if (bar_w > 0) {
                    gfx_draw_rect(119, 28, bar_w, 4, COLOR_CYAN);
                }
                if (bar_w < 112) {
                    gfx_draw_rect(119 + bar_w, 28, 112 - bar_w, 4, COLOR_CARD_BG);
                }
            } else if (bar_w > prev_bar_w) {
                /* Incremental fill: only paint newly advanced columns! Zero flicker! */
                gfx_draw_rect(119 + prev_bar_w, 28, bar_w - prev_bar_w, 4, COLOR_CYAN);
            }
            prev_bar_w = bar_w;
        }

        /* 11. Notification Banner (SRAM Save / Load feedback) */
        if (save_notify_frames > 0) {
            save_notify_frames--;
            gfx_draw_rect(10, 98, 220, 10, COLOR_CARD_BORDER);
            gfx_draw_text(14, 99, notify_msg, COLOR_EMERALD, COLOR_CARD_BORDER, false);
            if (save_notify_frames == 0) {
                gfx_draw_rect(10, 98, 220, 10, COLOR_NAVY);
                reset_piano_visualizer_state();
            }
        }
    }
}
