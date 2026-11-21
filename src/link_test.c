#include "../include/link_test.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/multiboot.h"
#include "../include/multiboot_payload.h"
#include "../include/help_guide.h"
#include "../include/localization.h"

static const char* player_names[LANG_COUNT][4] = {
    /* LANG_DE */
    {
        "SPIELER 1 (MASTER) [LILA]",
        "SPIELER 2 (SLAVE 1) [GRAU]",
        "SPIELER 3 (SLAVE 2) [GRAU]",
        "SPIELER 4 (SLAVE 3) [GRAU]"
    },
    /* LANG_EN */
    {
        "PLAYER 1 (MASTER) [PURPLE]",
        "PLAYER 2 (SLAVE 1) [GREY]",
        "PLAYER 3 (SLAVE 2) [GREY]",
        "PLAYER 4 (SLAVE 3) [GREY]"
    }
};

static const char* const link_headers[LANG_COUNT] = {
    "LINKKABEL & SERIELLER PORT",
    "LINK CABLE & SERIAL PORT"
};

static const char* const link_subheaders[LANG_COUNT] = {
    "SIO-PORT",
    "SIO PORT"
};

static const char* const link_footers[LANG_COUNT] = {
    "Kabel stecken | SELECT:Hilfe | B:Ende",
    "Cable test | SELECT:Guide | B:Back"
};

static const char* const card1_titles[LANG_COUNT] = {
    "KABEL-STATUS & SPIELER-ROLLE",
    "CABLE STATUS & PLAYER ROLE"
};

static const char* const card2_titles[LANG_COUNT] = {
    "SITZUNGS- & BUS-TELEMETRIE",
    "SESSION & BUS TELEMETRY"
};

static const char* const card3_titles[LANG_COUNT] = {
    "KOMMUNIKATION & LIVE-LOG",
    "COMMUNICATION & EVENT LOG"
};

#define LOG_MAX 3
typedef struct {
    int tick;
    const char* de;
    const char* en;
} LogEntry;

static LogEntry s_log[LOG_MAX];
static int s_log_count = 0;
static bool s_log_dirty = true;

static void log_event(int tick, const char* de, const char* en) {
    if (s_log_count < LOG_MAX) {
        s_log[s_log_count].tick = tick;
        s_log[s_log_count].de = de;
        s_log[s_log_count].en = en;
        s_log_count++;
    } else {
        for (int i = 0; i < LOG_MAX - 1; i++) {
            s_log[i] = s_log[i + 1];
        }
        s_log[LOG_MAX - 1].tick = tick;
        s_log[LOG_MAX - 1].de = de;
        s_log[LOG_MAX - 1].en = en;
    }
    s_log_dirty = true;
}

static void draw_event_log(void) {
    gfx_draw_rect(8, 118, 224, 27, COLOR_CARD_BG);
    for (int i = 0; i < s_log_count; i++) {
        int sec = s_log[i].tick / 60;
        int m = sec / 60;
        int s = sec % 60;
        const char* msg = (g_lang == LANG_DE) ? s_log[i].de : s_log[i].en;
        u16 col = (i == s_log_count - 1) ? COLOR_WHITE : COLOR_LIGHT_GRAY;

        char line[40];
        line[0] = '[';
        line[1] = '0' + ((m / 10) % 10);
        line[2] = '0' + (m % 10);
        line[3] = ':';
        line[4] = '0' + ((s / 10) % 10);
        line[5] = '0' + (s % 10);
        line[6] = ']';
        line[7] = ' ';
        int p = 8;
        int q = 0;
        while (msg[q] && p < 36) {
            line[p++] = msg[q++];
        }
        line[p] = '\0';

        gfx_draw_text(8, 119 + i * 9, line, col, COLOR_CARD_BG, false);
    }
}

void run_link_test(void) {
    /* Initialize SIO to 16-bit Multi-Player Mode at 115200 baud */
    REG_RCNT = 0x0000;
    REG_SIOCNT = 0x2003;
    REG_SIOMLT_SEND = 0x0000;
    REG_SIOMULTI0 = 0x0000;
    REG_SIOMULTI1 = 0x0000;
    REG_SIOMULTI2 = 0x0000;
    REG_SIOMULTI3 = 0x0000;

    gfx_clear(COLOR_NAVY);
    gfx_draw_header(link_headers[g_lang], link_subheaders[g_lang]);
    gfx_draw_footer(link_footers[g_lang]);

    /* Card 1: Connection & Player Role */
    gfx_draw_card(4, 16, 232, 50, card1_titles[g_lang], COLOR_CARD_BORDER);

    /* Card 2: Session & Bus Telemetry */
    gfx_draw_card(4, 68, 232, 38, card2_titles[g_lang], COLOR_CARD_BORDER);

    /* Card 3: Real-Time Communication & Event Log */
    gfx_draw_card(4, 108, 232, 39, card3_titles[g_lang], COLOR_CARD_BORDER);

    int locked_role = -1;  /* -1: Unassigned/Disconnected, 0: Player 1 (Master), 1: Player 2 (Slave 1) */
    int slave_id = 1;     /* Default to Player 2 (Slave 1) */
    int partner_timeout = 0;
    u16 tx_seq = 1;
    u16 slave_seq = 1;
    u16 my_last_tx = 0;
    u16 last_rx_word = 0;
    u32 packet_count = 0;
    int tick = 0;

    bool mb_slave_detected = false;
    bool prev_mb_slave = false;

    /* Previous display states for flicker-free redrawing */
    int prev_role_id = -99;
    bool prev_conn = false;
    u32 prev_pkt = 0xFFFFFFFF;
    int prev_si = -1;
    int prev_sd = -1;
    Language prev_lang = g_lang;

    /* Initialize Event Log */
    s_log_count = 0;
    log_event(0, "SIO aktiv (115200 Baud)", "SIO active (115200 baud)");
    log_event(0, "Warte auf Verbindung...", "Waiting for partner...");

    while (1) {
        vsync();
        input_poll();
        tick++;

        bool cable_connected = (partner_timeout > 0);

        if (key_hit(KEY_B)) {
            /* Reset SIO before leaving */
            REG_RCNT = 0x0000;
            REG_SIOCNT = 0x0000;
            break;
        }

        if (key_hit(KEY_SELECT)) {
            run_help_guide(HELP_TOPIC_LINK);
            gfx_clear(COLOR_NAVY);
            gfx_draw_header(link_headers[g_lang], link_subheaders[g_lang]);
            gfx_draw_footer(link_footers[g_lang]);
            gfx_draw_card(4, 16, 232, 50, card1_titles[g_lang], COLOR_CARD_BORDER);
            gfx_draw_card(4, 68, 232, 38, card2_titles[g_lang], COLOR_CARD_BORDER);
            gfx_draw_card(4, 108, 232, 39, card3_titles[g_lang], COLOR_CARD_BORDER);
            prev_role_id = -99;
            prev_conn = !cable_connected;
            prev_pkt = 0xFFFFFFFF;
            prev_si = -1;
            prev_sd = -1;
            prev_lang = g_lang;
            s_log_dirty = true;
            continue;
        }

        u16 siocnt = REG_SIOCNT;

        /* 1. Physical Pin Sampling:
              Pin 3 (SI): On Purple plug (Master), hardwired to GND (Bit 2 == 0).
                          On Grey plug (Slave), driven HIGH by Master SO (Bit 2 == 1).
                          When unplugged, floats (reads 0 or 1 unpredictably).
              Pin 4 (SD): Pulled HIGH automatically by GBA in Multi-Player mode. */
        bool raw_si_low = ((siocnt & (1 << 2)) == 0);
        bool raw_sd = ((siocnt & (1 << 3)) != 0);

        /* 2. Communication & State Machine */
        /* MultiBoot Trigger: If a cartridgeless GBA is detected in BIOS, user can press [A] */
        if (!cable_connected && mb_slave_detected && key_hit(KEY_A)) {
            int size_kb = (MULTIBOOT_PAYLOAD_SIZE + 1023) / 1024;
            gfx_draw_rect(8, 28, 224, 36, COLOR_CARD_BG);
            if (g_lang == LANG_DE) {
                gfx_draw_text(8, 29, "MULTIBOOT: SENDE CLIENT-SUITE...", COLOR_GOLD, COLOR_CARD_BG, false);
                gfx_draw_text_fmt(8, 41, COLOR_CYAN, COLOR_CARD_BG, false,
                                  "Sende %d KB ins GBA 2 EWRAM...", size_kb);
                gfx_draw_text(8, 53, "Bitte warten (~6-7 Sekunden)...", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
            } else {
                gfx_draw_text(8, 29, "MULTIBOOT: TRANSMITTING CLIENT...", COLOR_GOLD, COLOR_CARD_BG, false);
                gfx_draw_text_fmt(8, 41, COLOR_CYAN, COLOR_CARD_BG, false,
                                  "Uploading %d KB to GBA 2 EWRAM...", size_kb);
                gfx_draw_text(8, 53, "Please wait (~6-7 seconds)...", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
            }

            log_event(tick, "Sende MultiBoot Client-Suite...", "Sending MultiBoot client suite...");

            bool ok = multiboot_send_payload(multiboot_payload, MULTIBOOT_PAYLOAD_SIZE);
            if (ok) {
                gfx_draw_rect(8, 28, 224, 36, COLOR_CARD_BG);
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 29, "MULTIBOOT: ERFOLG! CLIENT LAEUFT!", COLOR_EMERALD, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 41, "GBA 2 startet Suite im EWRAM...", COLOR_WHITE, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 53, "Verbinde mit dem SIO-Bus...", COLOR_GOLD, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text(8, 29, "MULTIBOOT: SUCCESS! CLIENT BOOTED!", COLOR_EMERALD, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 41, "GBA 2 starting suite from EWRAM...", COLOR_WHITE, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 53, "Connecting to SIO bus...", COLOR_GOLD, COLOR_CARD_BG, false);
                }

                log_event(tick, "MultiBoot OK! GBA 2 EWRAM laeuft", "MultiBoot OK! Client booted");

                /* Wait ~40 frames for client to initialize Mode 3 & SIO */
                for (int w = 0; w < 40; w++) {
                    vsync();
                }
                mb_slave_detected = false;
                locked_role = 0;
                prev_conn = false;
                prev_role_id = -99;
            } else {
                gfx_draw_rect(8, 28, 224, 36, COLOR_CARD_BG);
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 29, "MULTIBOOT: FEHLGESCHLAGEN / TIMEOUT", COLOR_AMBER, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 41, "Kabel & GBA 2 Zustand pruefen", COLOR_CORAL, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 53, "Druecke [A] fuer erneuten Versuch", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text(8, 29, "MULTIBOOT: FAILED / TIMEOUT", COLOR_AMBER, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 41, "Check cable & GBA 2 power status", COLOR_CORAL, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 53, "Press [A] to retry transfer", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                }

                log_event(tick, "MultiBoot fehlgeschlagen", "MultiBoot failed / timeout");

                for (int w = 0; w < 30; w++) {
                    vsync();
                }
            }
        }

        /* 3. Communication & Handshake Execution */
        if (locked_role == 0) {
            /* FIRMLY LOCKED MASTER: Active connection, ping partner every 4 frames (~15 Hz) */
            if ((tick & 3) == 0 && !(REG_SIOCNT & 0x0080)) {
                if (REG_SIOCNT & 0x0040) {
                    REG_SIOCNT = 0x2003;
                }

                u16 tx_val = (u16)(0x1000 | (tx_seq & 0x0FFF));
                my_last_tx = tx_val;
                REG_SIOMLT_SEND = tx_val;
                REG_SIOCNT = 0x2083; /* Start Multi-Player transfer at 115200 baud */

                int timeout = 10000;
                while ((REG_SIOCNT & 0x0080) && --timeout > 0);

                if (timeout > 0 && !(REG_SIOCNT & 0x0040)) {
                    u16 rx1 = REG_SIOMULTI1;
                    if (rx1 != 0xFFFF && (rx1 & 0xF000) == 0x2000) {
                        partner_timeout = 60; /* Verified partner: keep alive 1.0 second */
                        last_rx_word = rx1;
                        packet_count++;
                        tx_seq++;
                    }
                } else {
                    REG_SIOCNT = 0x2003;
                }
            }
        } else if (locked_role == 1) {
            /* FIRMLY LOCKED SLAVE: Listen for Master clock */
            u16 my_keys = ~REG_KEYINPUT & 0x03FF;
            u16 tx_val = (u16)(0x2000 | (my_keys & 0x03FF));
            REG_SIOMLT_SEND = tx_val;

            if (!(REG_SIOCNT & 0x0080)) {
                if (REG_SIOCNT & 0x0040) {
                    REG_SIOCNT = 0x2003;
                } else {
                    int hw_id = (REG_SIOCNT >> 4) & 3;
                    if (hw_id >= 1) {
                        u16 rx0 = REG_SIOMULTI0;
                        if (rx0 != 0xFFFF && ((rx0 & 0xF000) == 0x1000 || rx0 == 0x6200) &&
                            rx0 != my_last_tx && rx0 != last_rx_word) {
                            partner_timeout = 60;
                            slave_id = hw_id;
                            last_rx_word = rx0;
                            packet_count++;
                            slave_seq++;
                        }
                    }
                }
            }
        } else {
            /* STANDBY / SEARCHING FOR PARTNER:
               1. Keep keys in SIOMLT_SEND in case an external Master clocks us */
            u16 my_keys = ~REG_KEYINPUT & 0x03FF;
            REG_SIOMLT_SEND = (u16)(0x2000 | (my_keys & 0x03FF));

            /* Check if an external Master clocked us */
            if (!(REG_SIOCNT & 0x0080)) {
                if (REG_SIOCNT & 0x0040) {
                    REG_SIOCNT = 0x2003;
                } else {
                    int hw_id = (REG_SIOCNT >> 4) & 3;
                    if (hw_id >= 1 && tick > 20) {
                        u16 rx0 = REG_SIOMULTI0;
                        if (rx0 != 0xFFFF && (rx0 & 0xF000) == 0x1000 && rx0 != last_rx_word) {
                            partner_timeout = 60;
                            locked_role = 1; /* Lock as Slave */
                            slave_id = hw_id;
                            last_rx_word = rx0;
                            packet_count++;
                            slave_seq++;
                        }
                    }
                }
            }

            /* 2. Every 8 frames, probe for a GBA in BIOS (MultiBoot) or running client */
            if (locked_role < 0 && (tick & 7) == 0 && !(REG_SIOCNT & 0x0080)) {
                u16 probe_rx = 0;
                MultiBootDetectResult res = multiboot_detect_slave(&probe_rx);
                if (res == MB_DETECT_BIOS) {
                    if (!mb_slave_detected) {
                        log_event(tick, "GBA 2 BIOS erkannt (bereit)", "GBA 2 BIOS detected (ready)");
                    }
                    mb_slave_detected = true;
                } else if (res == MB_DETECT_CLIENT) {
                    mb_slave_detected = false;
                    partner_timeout = 60;
                    locked_role = 0; /* Lock as Master */
                    last_rx_word = probe_rx;
                    packet_count++;
                    tx_seq++;
                    log_event(tick, "GBA 2 Client: Master aktiv", "GBA 2 client: Master active");
                } else {
                    mb_slave_detected = false;
                    REG_SIOCNT = 0x2003;
                }
            }
        }

        /* 4. Watchdog Countdown & Disconnect Detection:
              Connection status is strictly bound to confirmed packet exchange.
              When no packets arrive for 60 consecutive frames (1.0 s),
              the cable is considered unplugged. */
        cable_connected = (partner_timeout > 0);
        if (partner_timeout > 0) {
            partner_timeout--;
            if (partner_timeout == 0) {
                /* Connection lost */
                locked_role = -1;
                slave_id = 1;
                last_rx_word = 0;
                packet_count = 0;
                mb_slave_detected = false;
                REG_SIOCNT = 0x2003;
                log_event(tick, "Verbindung getrennt / Timeout", "Link disconnected / timeout");
            }
        }

        /* Log connection trigger */
        if (cable_connected && !prev_conn) {
            log_event(tick, "Link synchron: Datenaustausch aktiv", "Link synced: Data active");
        }

        int role_id = cable_connected ? (locked_role == 0 ? 0 : slave_id) : -1;

        /* If language changed while away in guide, redraw cards */
        if (g_lang != prev_lang) {
            prev_lang = g_lang;
            gfx_clear(COLOR_NAVY);
            gfx_draw_header(link_headers[g_lang], link_subheaders[g_lang]);
            gfx_draw_footer(link_footers[g_lang]);
            gfx_draw_card(4, 16, 232, 50, card1_titles[g_lang], COLOR_CARD_BORDER);
            gfx_draw_card(4, 68, 232, 38, card2_titles[g_lang], COLOR_CARD_BORDER);
            gfx_draw_card(4, 108, 232, 39, card3_titles[g_lang], COLOR_CARD_BORDER);
            prev_role_id = -99;
            prev_pkt = 0xFFFFFFFF;
            prev_si = -1;
            prev_sd = -1;
            s_log_dirty = true;
        }

        /* 5. Redraw Card 1: Cable Status & Player Role */
        if (role_id != prev_role_id || cable_connected != prev_conn ||
            mb_slave_detected != prev_mb_slave) {

            prev_role_id = role_id;
            prev_conn = cable_connected;
            prev_mb_slave = mb_slave_detected;

            gfx_draw_rect(8, 28, 224, 36, COLOR_CARD_BG);

            if (cable_connected) {
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 29, "LINKKABEL: VERBUNDEN (SYNCHRON)", COLOR_EMERALD, COLOR_CARD_BG, false);
                    if (locked_role == 0) {
                        gfx_draw_text(8, 41, "ROLLE: SPIELER 1 (MASTER / HOST)", COLOR_CYAN, COLOR_CARD_BG, false);
                    } else {
                        gfx_draw_text_fmt(8, 41, COLOR_GOLD, COLOR_CARD_BG, false,
                                          "ROLLE: %s", player_names[LANG_DE][slave_id]);
                    }
                    gfx_draw_text(8, 53, "Partner: ONLINE | SIO: 115200 bps", COLOR_EMERALD, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text(8, 29, "LINK CABLE: CONNECTED (SYNCED)", COLOR_EMERALD, COLOR_CARD_BG, false);
                    if (locked_role == 0) {
                        gfx_draw_text(8, 41, "ROLE: PLAYER 1 (MASTER / HOST)", COLOR_CYAN, COLOR_CARD_BG, false);
                    } else {
                        gfx_draw_text_fmt(8, 41, COLOR_GOLD, COLOR_CARD_BG, false,
                                          "ROLE: %s", player_names[LANG_EN][slave_id]);
                    }
                    gfx_draw_text(8, 53, "Partner: ONLINE | SIO: 115200 bps", COLOR_EMERALD, COLOR_CARD_BG, false);
                }
            } else if (mb_slave_detected) {
                int size_kb = (MULTIBOOT_PAYLOAD_SIZE + 1023) / 1024;
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 29, "GBA 2 IM BIOS (MULTIBOOT BEREIT)", COLOR_GOLD, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 41, "DRUECKE [A]: SUITE UEBERTRAGEN", COLOR_EMERALD, COLOR_CARD_BG, false);
                    gfx_draw_text_fmt(8, 53, COLOR_CYAN, COLOR_CARD_BG, false,
                                      "%d KB Diagnose-Suite (~6-7s)", size_kb);
                } else {
                    gfx_draw_text(8, 29, "GBA 2 IN BIOS (MULTIBOOT READY)", COLOR_GOLD, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 41, "PRESS [A] TO TRANSMIT SUITE", COLOR_EMERALD, COLOR_CARD_BG, false);
                    gfx_draw_text_fmt(8, 53, COLOR_CYAN, COLOR_CARD_BG, false,
                                      "Uploads %d KB suite (~6-7 sec)", size_kb);
                }
            } else {
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 29, "LINKKABEL: WARTE AUF VERBINDUNG", COLOR_AMBER, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 41, "Kabel stecken & GBA 2 einschalten", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 53, "1-Modul: Lila in GBA1, Grau in GBA2", COLOR_CYAN, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text(8, 29, "LINK CABLE: WAITING FOR PARTNER", COLOR_AMBER, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 41, "Connect cable & turn on GBA 2", COLOR_LIGHT_GRAY, COLOR_CARD_BG, false);
                    gfx_draw_text(8, 53, "1-Cart: Purple in GBA1, Grey in GBA2", COLOR_CYAN, COLOR_CARD_BG, false);
                }
            }
        }

        /* 6. Redraw Card 2: Session & Bus Telemetry */
        int cur_si = raw_si_low ? 0 : 1;
        int cur_sd = raw_sd ? 1 : 0;

        if (packet_count != prev_pkt || cur_si != prev_si || cur_sd != prev_sd || (tick & 31) == 0) {
            prev_pkt = packet_count;
            prev_si = cur_si;
            prev_sd = cur_sd;

            gfx_draw_rect(8, 79, 224, 25, COLOR_CARD_BG);

            if (cable_connected) {
                if (g_lang == LANG_DE) {
                    gfx_draw_text_fmt(8, 80, COLOR_EMERALD, COLOR_CARD_BG, false,
                                      "Pakete: %5u | 115200 Baud | Sync: OK", packet_count);
                    gfx_draw_text(8, 91, "GBA 2 Status: EWRAM Session aktiv", COLOR_CYAN, COLOR_CARD_BG, false);
                } else {
                    gfx_draw_text_fmt(8, 80, COLOR_EMERALD, COLOR_CARD_BG, false,
                                      "Packets: %5u | 115k Baud | Sync: OK", packet_count);
                    gfx_draw_text(8, 91, "GBA 2 Status: EWRAM session active", COLOR_CYAN, COLOR_CARD_BG, false);
                }
            } else if (mb_slave_detected) {
                int size_kb = (MULTIBOOT_PAYLOAD_SIZE + 1023) / 1024;
                if (g_lang == LANG_DE) {
                    gfx_draw_text(8, 80, "BIOS-Handshake: 0x6200 bestaetigt", COLOR_GOLD, COLOR_CARD_BG, false);
                    gfx_draw_text_fmt(8, 91, COLOR_CYAN, COLOR_CARD_BG, false,
                                      "Payload bereit: %d KB EWRAM-Suite", size_kb);
                } else {
                    gfx_draw_text(8, 80, "BIOS Handshake: 0x6200 confirmed", COLOR_GOLD, COLOR_CARD_BG, false);
                    gfx_draw_text_fmt(8, 91, COLOR_CYAN, COLOR_CARD_BG, false,
                                      "Payload ready: %d KB EWRAM suite", size_kb);
                }
            } else {
                if (g_lang == LANG_DE) {
                    const char* si_str = raw_si_low ? "LOW(Host)" : "HIGH(Pull)";
                    const char* sd_str = raw_sd ? "HIGH(OK)" : "LOW(Idle)";
                    gfx_draw_text(8, 80, "SIO-Modus: 16-Bit Multi-Player", COLOR_WHITE, COLOR_CARD_BG, false);
                    gfx_draw_text_fmt(8, 91, COLOR_LIGHT_GRAY, COLOR_CARD_BG, false,
                                      "Pins: SI:%-10s SD:%s", si_str, sd_str);
                } else {
                    const char* si_str = raw_si_low ? "LOW(Host)" : "HIGH(Pull)";
                    const char* sd_str = raw_sd ? "HIGH(OK)" : "LOW(Idle)";
                    gfx_draw_text(8, 80, "SIO Mode: 16-Bit Multi-Player", COLOR_WHITE, COLOR_CARD_BG, false);
                    gfx_draw_text_fmt(8, 91, COLOR_LIGHT_GRAY, COLOR_CARD_BG, false,
                                      "Pins: SI:%-10s SD:%s", si_str, sd_str);
                }
            }
        }

        /* 7. Redraw Card 3: Real-Time Event Log */
        if (s_log_dirty) {
            s_log_dirty = false;
            draw_event_log();
        }
    }
}
