#ifndef RTC_H
#define RTC_H

#include "gba.h"

typedef struct {
    bool present;
    u8 year;      /* 0..99 */
    u8 month;     /* 1..12 */
    u8 day;       /* 1..31 */
    u8 day_of_week; /* 0..6 */
    u8 hours;     /* 0..23 */
    u8 minutes;   /* 0..59 */
    u8 seconds;   /* 0..59 */
} RtcTime;

/* Probe for RTC presence and read current time */
bool rtc_probe(void);
bool rtc_read_time(RtcTime* out_time);

/* Cartridge Hardware Rumble Control (GPIO Bit 3 on Drill Dozer / EZODE / insideGadgets) */
void rumble_set(bool on);
bool rumble_is_active(void);

#endif /* RTC_H */
