#ifndef SOUND_H
#define SOUND_H

#include "gba.h"

/* Sound Subsystem */
void sound_init(void);
void sound_stop_all(void);

/* Sound Triggers */
void sound_play_square1(u16 freq_val, u8 duty, u8 sweep_shift, bool sweep_down, u8 pan_left_right);
void sound_play_square2(u16 freq_val, u8 duty, u8 pan_left_right);
void sound_play_wave(u16 freq_val, int waveform_type, u8 pan_left_right);
void sound_play_noise(u8 clock_div, bool seven_step, u8 pan_left_right);

/* Panning: 1 = Left, 2 = Right, 3 = Center/Both */
#define PAN_LEFT   1
#define PAN_RIGHT  2
#define PAN_CENTER 3
void sound_set_pan(int channel, u8 pan);

/* DirectSound 8-bit PCM */
void sound_play_directsound_a(const s8* samples, int count, u8 pan, u8 vol);
void sound_play_directsound_a_note(const s8* samples, int count, u16 timer_reload, u8 pan, u8 vol);
void sound_stop_directsound_a(void);

void sound_play_directsound_b(const s8* samples, int count, u8 pan, u8 vol);
void sound_play_directsound_b_note(const s8* samples, int count, u16 timer_reload, u8 pan, u8 vol);
void sound_stop_directsound_b(void);

void sound_stop_directsound(void);

/* Interactive Sound Test Screen */
void run_audio_test(void);

#endif /* SOUND_H */
