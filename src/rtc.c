#include "../include/rtc.h"

#define REG_RTC_DATA   (*(vu16*)0x080000C4)
#define REG_RTC_DIR    (*(vu16*)0x080000C6)
#define REG_RTC_ENABLE (*(vu16*)0x080000C8)

#define RTC_PIN_SCK    (1 << 0)
#define RTC_PIN_SIO    (1 << 1)
#define RTC_PIN_CS     (1 << 2)
#define RTC_PIN_RUMBLE (1 << 3)

/* State of cartridge hardware rumble motor */
static bool s_rumble_active = false;

/* Helper to write REG_RTC_DATA while preserving the Rumble Motor state */
static inline void rtc_write_data(u16 val) {
    if (s_rumble_active) {
        val |= RTC_PIN_RUMBLE;
    } else {
        val &= ~RTC_PIN_RUMBLE;
    }
    REG_RTC_DATA = val;
}

/* Nintendo SIIRTC signature string for emulator and flashcart autodetection */
const char rtc_signature[] __attribute__((used)) = "SIIRTC_V001";

static inline void rtc_delay(void) {
    for (volatile int i = 0; i < 30; i++);
}

/* Transmit 8-bit command byte MSB-first according to Seiko S-3511A specification */
static void rtc_out_cmd(u8 cmd) {
    REG_RTC_DIR = RTC_PIN_CS | RTC_PIN_SCK | RTC_PIN_SIO | RTC_PIN_RUMBLE;
    rtc_write_data(RTC_PIN_SCK); /* CS=0, SCK=1 */
    rtc_delay();
    rtc_write_data(RTC_PIN_CS | RTC_PIN_SCK); /* CS=1, SCK=1 */
    rtc_delay();

    for (int i = 7; i >= 0; i--) {
        u16 bit = (cmd & (1 << i)) ? RTC_PIN_SIO : 0;
        rtc_write_data(RTC_PIN_CS | bit); /* SCK=0 */
        rtc_delay();
        rtc_write_data(RTC_PIN_CS | RTC_PIN_SCK | bit); /* SCK=1 */
        rtc_delay();
    }
}

/* Receive 8-bit parameter data byte LSB-first */
static u8 rtc_in_byte(void) {
    REG_RTC_DIR = RTC_PIN_CS | RTC_PIN_SCK | RTC_PIN_RUMBLE; /* SIO as input, Rumble as output */
    u8 ret = 0;

    for (int i = 0; i < 8; i++) {
        rtc_write_data(RTC_PIN_CS); /* SCK=0 */
        rtc_delay();
        rtc_write_data(RTC_PIN_CS | RTC_PIN_SCK); /* SCK=1 */
        rtc_delay();

        if (REG_RTC_DATA & RTC_PIN_SIO) {
            ret |= (1 << i);
        }
    }
    return ret;
}

/* Write 8-bit parameter data byte LSB-first */
static void rtc_out_byte(u8 val) {
    REG_RTC_DIR = RTC_PIN_CS | RTC_PIN_SCK | RTC_PIN_SIO | RTC_PIN_RUMBLE;
    for (int i = 0; i < 8; i++) {
        u16 bit = (val & (1 << i)) ? RTC_PIN_SIO : 0;
        rtc_write_data(RTC_PIN_CS | bit); /* SCK=0 */
        rtc_delay();
        rtc_write_data(RTC_PIN_CS | RTC_PIN_SCK | bit); /* SCK=1 */
        rtc_delay();
    }
}

/* Terminate RTC communication and deassert CS */
static void rtc_end(void) {
    REG_RTC_DIR = RTC_PIN_CS | RTC_PIN_SCK | RTC_PIN_SIO | RTC_PIN_RUMBLE;
    rtc_write_data(RTC_PIN_SCK); /* CS=0, SCK=1 */
    rtc_delay();
    rtc_write_data(0);           /* CS=0, SCK=0 */
    /* Keep REG_RTC_ENABLE = 0x0001 so rumble / GPIO remains active */
    REG_RTC_ENABLE = 0x0001;
}

static inline bool is_valid_bcd(u8 bcd) {
    return ((bcd & 0x0F) <= 9) && ((bcd >> 4) <= 9);
}

static inline u8 bcd_to_dec(u8 bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

bool rtc_probe(void) {
    RtcTime t;
    return rtc_read_time(&t);
}

bool rtc_read_time(RtcTime* out_time) {
    if (!out_time) return false;

    u16 ime = REG_IME;
    REG_IME = 0; /* Atomic transfer: protect from interrupt preemption */

    /* 1. Read Status Register 2 (Command 0x63) to check 24h mode and validity */
    REG_RTC_ENABLE = 0x0001;
    rtc_out_cmd(0x63);
    u8 stat2 = rtc_in_byte();
    rtc_end();

    /* Check if open bus (floating pins return 0xFF or 0x00) */
    if (stat2 == 0xFF) {
        REG_IME = ime;
        out_time->present = false;
        return false;
    }

    /* If not in 24h mode (bit 6 = 0), configure 24h mode (Command 0x62, write 0x40) */
    if (!(stat2 & 0x40)) {
        REG_RTC_ENABLE = 0x0001;
        rtc_out_cmd(0x62);
        rtc_out_byte(0x40);
        rtc_end();

        /* Re-read status */
        REG_RTC_ENABLE = 0x0001;
        rtc_out_cmd(0x63);
        stat2 = rtc_in_byte();
        rtc_end();
    }

    /* 2. Read 7-byte Date & Time (Command 0x65) */
    REG_RTC_ENABLE = 0x0001;
    rtc_out_cmd(0x65);

    u8 raw[7];
    for (int i = 0; i < 7; i++) {
        raw[i] = rtc_in_byte();
    }
    rtc_end();

    REG_IME = ime;

    /* Validate BCD integrity */
    u8 y_raw   = raw[0];
    u8 m_raw   = raw[1] & 0x1F;
    u8 d_raw   = raw[2] & 0x3F;
    u8 dow_raw = raw[3] & 0x07;
    u8 h_raw   = raw[4];
    u8 min_raw = raw[5] & 0x7F;
    u8 sec_raw = raw[6] & 0x7F;

    if (!is_valid_bcd(y_raw) || !is_valid_bcd(m_raw) || !is_valid_bcd(d_raw) ||
        !is_valid_bcd(min_raw) || !is_valid_bcd(sec_raw)) {
        out_time->present = false;
        return false;
    }

    u8 y = bcd_to_dec(y_raw);
    u8 m = bcd_to_dec(m_raw);
    u8 d = bcd_to_dec(d_raw);
    u8 dow = dow_raw;
    u8 min = bcd_to_dec(min_raw);
    u8 sec = bcd_to_dec(sec_raw);

    /* Hour decoding */
    u8 h = 0;
    if (stat2 & 0x40) {
        /* 24-hour mode */
        if (!is_valid_bcd(h_raw & 0x3F)) {
            out_time->present = false;
            return false;
        }
        h = bcd_to_dec(h_raw & 0x3F);
    } else {
        /* 12-hour mode: bit 7 is AM/PM (1 = PM, 0 = AM) on GBA S-3511A */
        if (!is_valid_bcd(h_raw & 0x3F)) {
            out_time->present = false;
            return false;
        }
        h = bcd_to_dec(h_raw & 0x3F);
        bool is_pm = (h_raw & 0x80) != 0;
        if (is_pm && h < 12) {
            h += 12;
        } else if (!is_pm && h == 12) {
            h = 0;
        }
    }

    /* Calendar range validation */
    if (m == 0 || m > 12 || d == 0 || d > 31 || h > 23 || min > 59 || sec > 59) {
        out_time->present = false;
        return false;
    }

    out_time->present = true;
    out_time->year = y;
    out_time->month = m;
    out_time->day = d;
    out_time->day_of_week = dow;
    out_time->hours = h;
    out_time->minutes = min;
    out_time->seconds = sec;

    return true;
}

void rumble_set(bool on) {
    s_rumble_active = on;
    REG_RTC_ENABLE = 0x0001;
    REG_RTC_DIR = RTC_PIN_CS | RTC_PIN_SCK | RTC_PIN_SIO | RTC_PIN_RUMBLE;
    rtc_write_data(0);
}

bool rumble_is_active(void) {
    return s_rumble_active;
}

