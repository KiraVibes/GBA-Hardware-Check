#ifndef INPUT_H
#define INPUT_H

#include "gba.h"

/* Button indices (0 to 9) */
#define BTN_IDX_A       0
#define BTN_IDX_B       1
#define BTN_IDX_SELECT  2
#define BTN_IDX_START   3
#define BTN_IDX_RIGHT   4
#define BTN_IDX_LEFT    5
#define BTN_IDX_UP      6
#define BTN_IDX_DOWN    7
#define BTN_IDX_R       8
#define BTN_IDX_L       9

typedef struct {
    u16 current;
    u16 previous;
    u16 pressed;
    u16 released;
    u32 press_counts[10];
    u32 hold_frames[10];
} InputState;

extern InputState g_input;

void input_init(void);
void input_poll(void);

static inline bool key_down(u16 mask) {
    return (g_input.current & mask) != 0;
}

static inline bool keys_all_down(u16 mask) {
    return (g_input.current & mask) == mask;
}

static inline bool key_hit(u16 mask) {
    return (g_input.pressed & mask) != 0;
}

static inline bool key_released(u16 mask) {
    return (g_input.released & mask) != 0;
}

/* Interactive gamepad hardware test screen */
void run_input_test(void);

#endif /* INPUT_H */
