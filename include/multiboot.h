#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "gba.h"

typedef struct {
    u32 reserved1[5];       /* 0x00..0x13 */
    u8  handshake_data;     /* 0x14 */
    u8  padding;            /* 0x15 */
    u16 handshake_timeout;  /* 0x16..0x17 */
    u8  probe_count;        /* 0x18 */
    u8  client_data[3];     /* 0x19..0x1B */
    u8  palette_data;       /* 0x1C */
    u8  response_bit;       /* 0x1D */
    u8  client_bit;         /* 0x1E */
    u8  reserved2;          /* 0x1F */
    const u8* boot_srcp;    /* 0x20 */
    const u8* boot_endp;    /* 0x24 */
    const u8* masterp;      /* 0x28 */
    const u8* reserved3[3]; /* 0x2C..0x37 */
    u32 system_work2[4];    /* 0x38..0x47 */
    u8  sendflag;           /* 0x48 */
    u8  probe_target_bit;   /* 0x49 */
    u8  check_wait;         /* 0x4A */
    u8  server_type;        /* 0x4B */
} __attribute__((packed, aligned(4))) MultiBootParam;

typedef enum {
    MB_DETECT_NONE   = 0,
    MB_DETECT_BIOS   = 1, /* GBA 2 waiting in BIOS (replies 0x720x) */
    MB_DETECT_CLIENT = 2  /* GBA 2 running test client (replies 0x2xxx) */
} MultiBootDetectResult;

/* Probes whether a cartridgeless GBA is listening in BIOS for MultiBoot,
   or if a client is already running. Returns detection status. */
MultiBootDetectResult multiboot_detect_slave(u16* out_rx_word);

/* Performs the full MultiBoot sequence, sending the payload to GBA 2.
   Returns true on success (GBA 2 boots), false on error. */
bool multiboot_send_payload(const u8* payload, u32 payload_size);

#endif /* MULTIBOOT_H */
