#ifndef SYSINFO_H
#define SYSINFO_H

#include "gba.h"

typedef enum {
    MODEL_GBA_CLASSIC = 0, /* AGB-001 or AGS-001 / AGS-101 / Micro */
    MODEL_GAMEBOY_PLAYER,  /* DOL-017 */
    MODEL_NINTENDO_DS,     /* NTR-001 / USG-001 (SLOT-2 GBA Mode) */
    MODEL_UNKNOWN
} ConsoleModel;

typedef struct {
    ConsoleModel model;
    const char* model_name;
    u16 waitcnt;
    bool prefetch_enabled;
    u8 sram_waitstates;
    u8 rom_ws0_first;
    u8 rom_ws0_second;
    bool sram_detected;
    bool rtc_detected;
    char rom_title[13];
    char rom_code[5];
    char maker_code[3];
} SystemInfo;

void sysinfo_gather(SystemInfo* info);
void run_sysinfo_screen(void);

#endif /* SYSINFO_H */
