#include "../include/help_guide.h"
#include "../include/graphics.h"
#include "../include/input.h"
#include "../include/sound.h"
#include "../include/localization.h"

static const char* const topic_titles[LANG_COUNT][HELP_TOPIC_COUNT] = {
    /* LANG_DE */
    {
        "HARDWARE-ZUSTAND & AUDIT",
        "SYSTEM- & BUS-DIAGNOSE",
        "TASTEN & PIVOT-PRUEFUNG",
        "BILDSCHIRM & IPS/OLED",
        "AUDIO & DIRECTSOUND-SUITE",
        "BENCHMARKS & OSZILLATOR",
        "SPANNUNG & KONDENSATOREN",
        "AKKU-LAUFZEIT & ENTLADETEST",
        "LINKKABEL, SIO & MULTIBOOT",
        "EZ-FLASH OMEGA & RUMBLE"
    },
    /* LANG_EN */
    {
        "HARDWARE HEALTH & AUDIT",
        "SYSTEM & BUS DIAGNOSTICS",
        "GAMEPAD & PIVOT CHECK",
        "DISPLAY QUALITY & IPS/OLED",
        "AUDIO & DIRECTSOUND SUITE",
        "BENCHMARKS & OSCILLATOR",
        "POWER-RAIL & RECAP TEST",
        "BATTERY RUNTIME & ENDURANCE",
        "LINK CABLE, SIO & MULTIBOOT",
        "EZ-FLASH OMEGA & RUMBLE"
    }
};

static const char* const topic_subtitles[LANG_COUNT][HELP_TOPIC_COUNT] = {
    /* LANG_DE */
    { "THEMA 1/10", "THEMA 2/10", "THEMA 3/10", "THEMA 4/10", "THEMA 5/10", "THEMA 6/10", "THEMA 7/10", "THEMA 8/10", "THEMA 9/10", "THEMA 10/10" },
    /* LANG_EN */
    { "TOPIC 1/10", "TOPIC 2/10", "TOPIC 3/10", "TOPIC 4/10", "TOPIC 5/10", "TOPIC 6/10", "TOPIC 7/10", "TOPIC 8/10", "TOPIC 9/10", "TOPIC 10/10" }
};

static const char* const guide_headers[LANG_COUNT] = {
    "HARDWARE-REFERENZ",
    "HARDWARE REFERENCE GUIDE"
};

static const char* const guide_footers[LANG_COUNT] = {
    "AUF/AB:Scroll | L/R:Thema | B:Zurueck",
    "UP/DN:Scroll | L/R:Topic | B:Back"
};

/* 0. Hardware Health & Audit Scorecard (<= 30 chars per line) */
static const char* const health_lines_de[] = {
    "[ AUTOMATISCHES AUDIT ]",
    "- Prueft CPU, IWRAM, EWRAM,",
    "  VRAM, Audio, SIO & SRAM.",
    "- Erkennt klemmende Tasten.",
    "- Berechnet 0-100% Zustand",
    "  und Konsolen-Zertifikat.",
    "",
    "[ KONSOLEN-BEWERTUNG ]",
    "- NOTE A+ (98-100%): PERFEKT.",
    "- NOTE A (88-97%): Sehr gut.",
    "- NOTE B (75-87%): Hinweis.",
    "- NOTE C (<75%): Fehler.",
    "",
    "[ AUDIT WIEDERHOLEN ]",
    "- Druecke A oder START fuer",
    "  erneute Konsolenpruefung."
};
#define HEALTH_DE_COUNT (sizeof(health_lines_de) / sizeof(health_lines_de[0]))

static const char* const health_lines_en[] = {
    "[ AUTOMATED HARDWARE AUDIT ]",
    "- Sweeps CPU, IWRAM, EWRAM,",
    "  VRAM, Audio, SIO & Save.",
    "- Checks stuck buttons & keys.",
    "- Computes 0-100% Health Score",
    "  and console certification.",
    "",
    "[ CONSOLE HEALTH GRADES ]",
    "- GRADE A+ (98-100%): MINT.",
    "- GRADE A (88-97%): Excellent.",
    "- GRADE B (75-87%): Advisory.",
    "- GRADE C (<75%): Fault found.",
    "",
    "[ RE-RUN AUDIT SWEEP ]",
    "- Press A or START anytime to",
    "  re-verify console integrity."
};
#define HEALTH_EN_COUNT (sizeof(health_lines_en) / sizeof(health_lines_en[0]))

/* 1. System Diagnostics (<= 30 chars per line) */
static const char* const sys_lines_de[] = {
    "[ MODELL-ERKENNUNG ]",
    "- Testet REG_WRAMCNT.",
    "- Original GBA, SP & Micro",
    "  erlauben Waitstate-Writes.",
    "- NDS ignoriert WRAMCNT im",
    "  GBA Slot-2 Modus.",
    "- MultiBoot: Code laeuft im",
    "  EWRAM ohne Steckmodul.",
    "",
    "[ WAITSTATES & BUS-TIMING ]",
    "- REG_WAITCNT (0x04000204)",
    "  steuert ROM- & SRAM-Takt.",
    "- WS0 Erstzugriff: 4,3,2,8 Zy",
    "- WS0 Folgetakt: 2 oder 1 Zyk.",
    "- Schnelles 50ns ROM: 3,1 Zy.",
    "- Standard-ROM: 4,2 Zyklen.",
    "- Prefetch puffert Thumb-Code.",
    "- Falsche Werte bremsen FPS.",
    "",
    "[ SPEICHER SRAM & FRAM ]",
    "- Prueft Adressraum 0x0E000000",
    "- Zerstoerungsfreier Bittest.",
    "- 'OK': Speicherchip intakt.",
    "- 'OFF': Flash/EEPROM oder",
    "  leerer Modulschacht.",
    "",
    "[ ECHTZEITUHR (RTC) ]",
    "- S-35180A RTC ueber GPIO.",
    "- Prueft 32.768 kHz Quarz.",
    "- Fehlt: CR1220 Batterie",
    "  pruefen oder tauschen.",
    "- Noetig fuer Pokemon-Zeit."
};
#define SYS_DE_COUNT (sizeof(sys_lines_de) / sizeof(sys_lines_de[0]))

static const char* const sys_lines_en[] = {
    "[ CONSOLE MODEL DETECTION ]",
    "- Tests REG_WRAMCNT.",
    "- Genuine GBA, SP & Micro",
    "  allow waitstate writes.",
    "- NDS ignores WRAMCNT in",
    "  GBA Slot-2 mode.",
    "- MultiBoot: Code runs from",
    "  EWRAM without cartridge.",
    "",
    "[ WAITSTATES & BUS TIMING ]",
    "- REG_WAITCNT (0x04000204)",
    "  sets ROM & SRAM speeds.",
    "- WS0 First: 4, 3, 2, 8 cyc.",
    "- WS0 Second: 2 or 1 cyc.",
    "- Fast 50ns ROM: 3,1 cyc.",
    "- Standard ROM: 4,2 cyc.",
    "- Prefetch: Automatically",
    "  buffers Thumb code.",
    "- Bad timings cause lag and",
    "  cut game framerates.",
    "",
    "[ SAVE SRAM & FRAM CHECK ]",
    "- Probes 0x0E000000 space.",
    "- Non-destructive bit test.",
    "- 'OK': Save chip clean.",
    "- 'OFF': Flash/EEPROM mode",
    "  or empty cart slot.",
    "",
    "[ EZ-FLASH RTC CLOCK ]",
    "- S-35180A RTC on GPIO.",
    "- Live clock verifies the",
    "  32.768 kHz quartz crystal.",
    "- If missing: Check or swap",
    "  the CR1220 battery.",
    "- Needed for Pokemon time."
};
#define SYS_EN_COUNT (sizeof(sys_lines_en) / sizeof(sys_lines_en[0]))

/* 2. Gamepad & Pivot Check (<= 30 chars per line) */
static const char* const pad_lines_de[] = {
    "[ TASTEN-ECHTZEITSTATUS ]",
    "- Erfasst alle 10 Tasten:",
    "  A, B, L, R, Start, Select,",
    "  sowie 4 Steuerkreuztasten.",
    "- Zaehler zeigen Tastenprellen",
    "- Soft-Reset: A+B+START+SEL.",
    "- B halten fuer Hauptmenue.",
    "",
    "[ D-PAD PIVOT-KUGEL TEST ]",
    "- Originale haben Mittelkugel.",
    "- Kugel blockiert HOCH+RUNTER",
    "  oder LINKS+RECHTS zeitgleich",
    "- Wenn beide gleichzeitig:",
    "  '!PIVOT FEHLER!' erscheint.",
    "- Ursache: Kugel abgebrochen,",
    "  weiche Gummis oder Gehaeuse.",
    "- Fuehrt zu Fehldiagonalen.",
    "",
    "[ DIAGONAL-SCHALTERTEST ]",
    "- Prueft alle 4 Diagonalen:",
    "  OL, OR, UL und UR.",
    "- BEIDE Richtungen muessen",
    "  gleichzeitig greifen.",
    "- Prueft Karbon-Kontaktpads.",
    "- Noetig fuer Metroid-Spruenge"
};
#define PAD_DE_COUNT (sizeof(pad_lines_de) / sizeof(pad_lines_de[0]))

static const char* const pad_lines_en[] = {
    "[ LIVE CONTROLLER STATE ]",
    "- Map tracks all 10 keys:",
    "  A, B, L, R, Start, Select,",
    "  and 4 D-Pad directions.",
    "- Counters expose bounce.",
    "- Soft-Reset: A+B+START+SEL.",
    "- Hold B to return to menu.",
    "",
    "[ D-PAD PIVOT INTEGRITY ]",
    "- Genuine D-pads use a center",
    "  ball pivot base.",
    "- Pivot blocks pressing UP+DN",
    "  or LEFT+RIGHT together.",
    "- If opposites press,",
    "  '!PIVOT ERR!' is shown.",
    "- Cause: Broken pivot ball,",
    "  worn rubber or bad shell.",
    "- Leads to false diagonals.",
    "",
    "[ DIAGONAL ACTUATION TEST ]",
    "- Tests all 4 diagonals:",
    "  UL, UR, DL, and DR.",
    "- BOTH directions must be",
    "  held together to PASS.",
    "- Tests carbon contact pads.",
    "- Vital for Metroid jumps."
};
#define PAD_EN_COUNT (sizeof(pad_lines_en) / sizeof(pad_lines_en[0]))

/* 3. Display Quality Tests (<= 30 chars per line) */
static const char* const disp_lines_de[] = {
    "[ PIXELFEHLER-TEST ]",
    "- 8 reine Vollflaechenfarben:",
    "  Weiss, Schwarz, Rot, Gruen,",
    "  Blau, Gelb, Cyan, Magenta.",
    "- SELECT: OSD ein/ausblenden.",
    "- Prueft 38.400 Pixel auf",
    "  tote oder haengende Pixel.",
    "- Taste A wechselt Farben.",
    "",
    "[ 15-BIT RGB-FARBVERLAEUFE ]",
    "- 32 Stufen pro Farbkanal:",
    "  Rot, Gruen, Blau, Grau.",
    "- Zeigt Kontrastkurven.",
    "- Entlarvt Banding und",
    "  IPS/OLED Schatten-Clipping.",
    "",
    "[ SCHLIEREN & REAKTIONSZEIT ]",
    "- Animierte Kontrastbalken.",
    "- Kantenunschaerfe pruefen:",
    "- Original AGB: ~60ms traege.",
    "- Modernes IPS: <5ms scharf.",
    "",
    "[ GEOMETRIE & AUSRICHTUNG ]",
    "- 16x16 Gitter & Fadenkreuz.",
    "- Prueft Display-Ausrichtung",
    "  in Zubehoer-Gehaeusen.",
    "",
    "[ VCOM FLICKER BIAS ]",
    "- Schachbrett-Flackertest.",
    "- SELECT: Vollbild anzeigen.",
    "- Falsche Bias-Spannung",
    "  erzeugt starkes Flimmern.",
    "- Poti am Mainboard justieren.",
    "",
    "[ 59.73 HZ IPS SCALER SYNC ]",
    "- Original GBA laeuft mit",
    "  59.728 Hz (280.896 Zyklen).",
    "- 60Hz-Scaler verlieren alle",
    "  3.6s einen Frame (Ruckler).",
    "- Drop-Zaehler zeigt Ruckler.",
    "",
    "[ VSYNC TEARING RASTER ]",
    "- Schnelle Balken entlarven",
    "  Tearing bei unsynchronen",
    "  Display-Flachbandkabeln.",
    "",
    "[ 30 HZ TRANSPARENZ-SCHATTEN ]",
    "- Retro-30Hz Flimmer-Effekt.",
    "- Gut: 50% weicher Schatten.",
    "- Schlecht: Flimmern/Schmieren"
};
#define DISP_DE_COUNT (sizeof(disp_lines_de) / sizeof(disp_lines_de[0]))

static const char* const disp_lines_en[] = {
    "[ DEAD PIXEL CHECKER ]",
    "- Cycles 8 pure colors:",
    "  White, Black, Red, Green,",
    "  Blue, Yellow, Cyan, Mag.",
    "- Press SELECT to hide OSD.",
    "- Check 38,400 pixels for",
    "  stuck or dead subpixels.",
    "- Press A to cycle colors.",
    "",
    "[ 15-BIT RGB & GRAY RAMPS ]",
    "- 32 steps per color band:",
    "  Red, Green, Blue, Gray.",
    "- Evaluates contrast curves.",
    "- Exposes color banding and",
    "  IPS/OLED shadow clipping.",
    "",
    "[ GHOSTING & RESPONSE TIME ]",
    "- Animated contrast bars.",
    "- Observe edge blur to test",
    "  panel response latency.",
    "- Original AGB: ~60ms blur.",
    "- Modern IPS: <5ms sharp.",
    "",
    "[ GEOMETRY & ALIGNMENT ]",
    "- 16x16 grid & crosshairs.",
    "- Tests lens alignment in",
    "  aftermarket shells.",
    "",
    "[ VCOM FLICKER BIAS ]",
    "- Checkerboard test pattern.",
    "- Press SELECT for full view.",
    "- Misadjusted bias voltage",
    "  creates heavy flicker.",
    "- Adjust mainboard pot.",
    "",
    "[ 59.73 HZ IPS SCALER SYNC ]",
    "- Genuine GBA outputs 59.728Hz",
    "  (280,896 cycles/frame).",
    "- Aftermarket 60.0Hz scalers",
    "  drop 1 frame every ~3.6s.",
    "- Drop counter exposes skips.",
    "",
    "[ VSYNC TEARING RASTER ]",
    "- Fast bars expose tearing",
    "  from unsynced ribbon cables.",
    "",
    "[ 30 HZ DROP SHADOW ]",
    "- Retro 30Hz flicker shadow.",
    "- Good 60Hz: 50% opacity blend",
    "- Bad: Heavy flicker/smearing."
};
#define DISP_EN_COUNT (sizeof(disp_lines_en) / sizeof(disp_lines_en[0]))

/* 4. Audio Channels & Stereo (<= 30 chars per line) */
static const char* const audio_lines_de[] = {
    "[ DMG HARDWARE-SYNTHESIZER ]",
    "- Kanal 1: Rechteck mit Sweep",
    "  und Lautstaerke-Huellkurve.",
    "- Kanal 2: Rechteckwelle",
    "  gestimmt auf 440 Hz (A4).",
    "- Kanal 3: 4-Bit Wave-RAM",
    "  mit 32-Stufen-Wellenform.",
    "- Kanal 4: LFSR Rauschkanal",
    "  fuer Drums & Soundeffekte.",
    "",
    "[ DIRECTSOUND 8-BIT PCM ]",
    "- Zwei 8-Bit DAC-Kanaele.",
    "- Hardware DMA1 FIFO Audio.",
    "- Fuer Sprache & Musik-Samples",
    "",
    "[ STEREO-KANAL-TRENNUNG ]",
    "- Testet Links, Rechts, Mitte.",
    "- SP Lautsprecher: Nur Mono.",
    "- Kopfhoerer: Echtes Stereo.",
    "",
    "[ KOPFHOERERBUCHSE FEHLER ]",
    "- Buchse hat Schaltkontakt.",
    "- 1 Seite leise: Port reinigen",
    "- Wenn Lautsprecher stumm",
    "  nach Ausstecken: Kontakt",
    "  im Klinkenstecker klemmt.",
    "",
    "[ RESONANZ & FREQUENZ-SWEEP ]",
    "- 50 Hz bis 10.000 Hz Sweep.",
    "- Findet Gehaeuse-Resonanzen.",
    "- 50-250 Hz: Schrauben oder",
    "  Batteriefach scheppert.",
    "- 300-1kHz: Membran gerissen",
    "  oder Spule schleift.",
    "- 1.5-4kHz: Displayglas- oder",
    "  Tasten-Vibrationen."
};
#define AUDIO_DE_COUNT (sizeof(audio_lines_de) / sizeof(audio_lines_de[0]))

static const char* const audio_lines_en[] = {
    "[ DMG HARDWARE SYNTH ]",
    "- Ch 1: Square with sweep",
    "  and volume envelope.",
    "- Ch 2: Melodic square wave",
    "  tuned to 440 Hz (A4).",
    "- Ch 3: 4-bit Wave RAM",
    "  with 32-sample wave.",
    "- Ch 4: LFSR white noise",
    "  for drums and SFX.",
    "",
    "[ DIRECTSOUND 8-BIT PCM ]",
    "- Dual 8-bit DAC channels.",
    "- Hardware DMA1 FIFO audio.",
    "- Used for voice & samples.",
    "",
    "[ STEREO SEPARATION ]",
    "- Tests Left, Right, Center.",
    "- SP Speaker: Hardware mono.",
    "- Headphone: True stereo.",
    "",
    "[ JACK TROUBLESHOOTING ]",
    "- Jack has leaf mute switch.",
    "- If 1 side quiet: clean jack.",
    "- If speaker silent on unplug:",
    "  leaf switch is stuck.",
    "",
    "[ SPEAKER & FREQUENCY SWEEP ]",
    "- 50 Hz to 10,000 Hz sweep.",
    "- Exposes casing resonances.",
    "- 50-250 Hz: Loose screws or",
    "  battery door rattle.",
    "- 300-1kHz: Torn speaker cone",
    "  or damaged voice coil.",
    "- 1.5-4kHz: Lens & key buzz."
};
#define AUDIO_EN_COUNT (sizeof(audio_lines_en) / sizeof(audio_lines_en[0]))

/* 5. Memory & CPU Benchmarks (<= 30 chars per line) */
static const char* const bench_lines_de[] = {
    "[ TAKTGEBER & TIMING ]",
    "- Misst CPU-Zyklen zwischen",
    "  zwei VBlank-Signalen.",
    "- Sollwert: ~280.896 Zyklen",
    "  (exakt 59.7275 Hz).",
    "- 16.78 MHz Quarz-Oszillator.",
    "- Abweichungen zeigen Drift.",
    "",
    "[ SPEICHER-DURCHSATZ ]",
    "- EWRAM: 256 KB On-Board RAM",
    "  16-Bit-Bus (2+1 Waitstates).",
    "- Typische Rate: ~3.5 MB/s.",
    "- IWRAM: 32 KB schneller RAM",
    "  ohne Waitstates (32-Bit).",
    "- ROM: Modul-Bus-Geschw.",
    "",
    "[ CPU ALU RECHENLEISTUNG ]",
    "- 10.000 32-Bit Mathe-Ops.",
    "- ROM-Bus (Prefetch): ~460k.",
    "- EWRAM-Bus: ~1.050.000 Zyklen",
    "- Beides gilt als PASS.",
    "",
    "[ RAM-BUS LEITUNGSTEST ]",
    "- Wandereins testet alle 16",
    "  Datenbus-Leitungen.",
    "- 0xAA55 und 0x55AA testen",
    "  Kurzschluesse der Pins.",
    "- Entlarvt kalte Loetstellen."
};
#define BENCH_DE_COUNT (sizeof(bench_lines_de) / sizeof(bench_lines_de[0]))

static const char* const bench_lines_en[] = {
    "[ OSCILLATOR & TIMING ]",
    "- Measures CPU cycles between",
    "  two VBlanks.",
    "- Target: ~280,896 cycles",
    "  (59.7275 Hz exact).",
    "- 16.78 MHz quartz crystal.",
    "- Deviations show drift.",
    "",
    "[ MEMORY BANDWIDTH ]",
    "- EWRAM: 256 KB on-board RAM",
    "  on 16-bit bus (2+1 wait).",
    "- Typical speed: ~3.5 MB/s.",
    "- IWRAM: 32 KB fast internal",
    "  zero-waitstate RAM.",
    "- ROM: Cartridge bus speed.",
    "",
    "[ CPU ALU THROUGHPUT ]",
    "- 10,000 32-bit math ops.",
    "- ROM Bus (Prefetch): ~460k.",
    "- EWRAM Bus: ~1,050,000 cyc.",
    "- Both are nominal PASS.",
    "",
    "[ RAM BUS INTEGRITY ]",
    "- Walking 1s tests all 16",
    "  data bus lines.",
    "- 0xAA55 and 0x55AA patterns",
    "  test adjacent pins.",
    "- Catches cold solder joints."
};
#define BENCH_EN_COUNT (sizeof(bench_lines_en) / sizeof(bench_lines_en[0]))

/* 6. Power-Rail & Capacitor Stress Test (<= 30 chars per line) */
static const char* const stress_lines_de[] = {
    "[ MAXIMALER STROMVERBRAUCH ]",
    "- 100% CPU 3D-Wuerfel-Matrix",
    "  Drahtgitter-Berechnung.",
    "- Zeitgleicher 4-Kanal-Ton",
    "  bei voller Lautstaerke.",
    "- Belastet internen DC-DC",
    "  Spannungswandler.",
    "",
    "[ SPANNUNGSABFALL & ROTE LED ]",
    "- Power-LED genau beobachten.",
    "- Flackert die LED bei voller",
    "  Batterie waehrend des Tests:",
    "- 1. Verschmutzter Schalter:",
    "  Kontakte mit IPA reinigen.",
    "- 2. Gealterte Elkos:",
    "  C35, C36, C37 beim AGB",
    "  (oder C33 beim SP) tauschen.",
    "",
    "[ TYPISCHE ELKO-DEFEKTE ]",
    "- Brummen & Rauschen im Ton.",
    "- Neustarts bei hellen IPS.",
    "- Absturz bei Rumble-Motoren."
};
#define STRESS_DE_COUNT (sizeof(stress_lines_de) / sizeof(stress_lines_de[0]))

static const char* const stress_lines_en[] = {
    "[ MAX POWER DRAIN LOAD ]",
    "- 100% CPU 3D cube matrix",
    "  wireframe transformation.",
    "- Simultaneous 4-channel",
    "  audio load at full volume.",
    "- Stresses internal DC-DC",
    "  voltage regulator.",
    "",
    "[ VOLTAGE DROP & RED LED ]",
    "- Watch Power LED closely.",
    "- If LED flickers RED during",
    "  load with fresh battery:",
    "- 1. Dirty Power Switch:",
    "  Clean slider with IPA.",
    "- 2. Failing Electrolytics:",
    "  Recap C35, C36, C37 on",
    "  AGB-001 (or C33 on SP).",
    "",
    "[ SYMPTOMS OF BAD CAPS ]",
    "- Buzzing / hissing audio.",
    "- IPS screen reboot loops.",
    "- Cart freeze on rumble."
};
#define STRESS_EN_COUNT (sizeof(stress_lines_en) / sizeof(stress_lines_en[0]))

/* 7. Battery Endurance & Run-Down (<= 30 chars per line) */
static const char* const battery_lines_de[] = {
    "[ AKKU-LAUFZEIT-BENCHMARK ]",
    "- Misst die exakte Laufzeit",
    "  einer Akkuladung.",
    "- Automatisches SRAM-Backup",
    "  im Sekundentakt.",
    "- Bleibt bei Abschaltung durch",
    "  leeren Akku erhalten.",
    "",
    "[ TEST-LASTPROFILE ]",
    "- 1. IDLE (DISPLAY-TEST):",
    "  Minimale Last, Display an.",
    "- 2. NORMAL (GAMEPLAY):",
    "  50% CPU-Last & Audio.",
    "- 3. MAX STRESS (VOLLLAST):",
    "  100% CPU 3D & 4-Kanal-Ton.",
    "",
    "[ VERGLEICHSWERTE (AGS-001) ]",
    "- Originalakku: ca. 7-10 Std.",
    "- IPS-Display Stufe 4: ~3.5-5h",
    "- Alter Akku: <2h (Tausch)",
    "",
    "[ ZEITBASIS & GENAUIGKEIT ]",
    "- RTC (Echtzeituhr): Erkennt",
    "  Cartridge-RTC (EZ-Flash/OEM)",
    "  fuer exakte Startzeit.",
    "- HW-Timer (16.384 kHz):",
    "  Interner GBA-Quarzkristall",
    "  zaehlt in Hardware exakt",
    "  1,000s ohne Frame-Drops."
};
#define BATTERY_DE_COUNT (sizeof(battery_lines_de) / sizeof(battery_lines_de[0]))

static const char* const battery_lines_en[] = {
    "[ BATTERY RUNTIME BENCHMARK ]",
    "- Measures exact battery life",
    "  on a single full charge.",
    "- Saves state to SRAM every",
    "  second automatically.",
    "- Preserves runtime when GBA",
    "  shuts down on low battery.",
    "",
    "[ WORKLOAD PROFILES ]",
    "- 1. IDLE (DISPLAY-ONLY):",
    "  Lowest power, screen on.",
    "- 2. NORMAL (GAMEPLAY):",
    "  50% CPU load & audio sfx.",
    "- 3. MAX STRESS (FULL LOAD):",
    "  100% CPU 3D & 4-ch audio.",
    "",
    "[ TYPICAL RUNTIMES (SP) ]",
    "- OEM Battery (600mAh): ~7-10h",
    "- IPS Mod Level 4: ~3.5-5h",
    "- Worn Battery: <2h (replace)",
    "",
    "[ TIMEBASE & PRECISION ]",
    "- RTC (Real-Time Clock):",
    "  Detects cart RTC (EZ/OEM)",
    "  for exact wall-clock sync.",
    "- HW-Timer (16.384 kHz):",
    "  Internal GBA quartz timer",
    "  increments in hardware every",
    "  1.000s with zero frame-drop."
};
#define BATTERY_EN_COUNT (sizeof(battery_lines_en) / sizeof(battery_lines_en[0]))

/* 8. Link Cable, SIO & MultiBoot (<= 30 chars per line) */
static const char* const link_lines_de[] = {
    "[ LINKKABEL & STECKERFARBEN ]",
    "- 16-Bit Multi-Player Modus",
    "  mit 115.200 Baud Takt.",
    "- Lila Stecker (Master/P1):",
    "  SI-Pin intern an GND.",
    "  Erzeugt das Taktsignal.",
    "- Grauer Stecker (Slave/P2-4):",
    "  Empfaengt das Taktsignal.",
    "",
    "[ 4-SPIELER KABEL SUPPORT ]",
    "- Voll kompatibel!",
    "- Lila Stecker in GBA 1,",
    "  Grauer Stecker in GBA 2.",
    "- MultiBoot kann an bis zu 3",
    "  GBAs zugleich senden!",
    "",
    "[ 1-MODUL MULTIBOOT-SUITE ]",
    "- GBA 2 startet ohne Modul.",
    "- GBA 1 uebertraegt die ~81 KB",
    "  Diagnose-Suite in ~6-7 Sek.",
    "- GBA 2 bootet direkt im RAM.",
    "",
    "[ TELEMETRIE & EREIGNIS-LOG ]",
    "- Echtzeit-Paketzaehler.",
    "- 3-zeiliges Live-Log zeigt",
    "  Bus-Synchronisation an."
};
#define LINK_DE_COUNT (sizeof(link_lines_de) / sizeof(link_lines_de[0]))

static const char* const link_lines_en[] = {
    "[ SERIAL I/O & CABLE PLUGS ]",
    "- 16-bit Multi-Player mode",
    "  at 115,200 baud.",
    "- Purple Plug (Master/P1):",
    "  SI pin wired to GND.",
    "  Generates clock signal.",
    "- Grey Plug (Slave/P2-P4):",
    "  Receives clock signal.",
    "",
    "[ 4-PLAYER CABLE SUPPORT ]",
    "- Fully compatible!",
    "- Purple plug in GBA 1,",
    "  Grey plug in GBA 2.",
    "- Can broadcast MultiBoot",
    "  to up to 3 GBAs at once!",
    "",
    "[ 1-CART MULTIBOOT SUITE ]",
    "- GBA 2 starts without cart.",
    "- GBA 1 transmits ~81 KB",
    "  diagnostic suite in ~6-7s.",
    "- GBA 2 boots 100% in RAM.",
    "",
    "[ TELEMETRY & EVENT LOG ]",
    "- Real-time packet count.",
    "- 3-line live event log",
    "  tracks bus synchronization."
};
#define LINK_EN_COUNT (sizeof(link_lines_en) / sizeof(link_lines_en[0]))

/* 8. EZ-Flash Omega & Rumble (<= 30 chars per line) */
static const char* const ezflash_lines_de[] = {
    "[ EZ-FLASH DE & RUMBLE ]",
    "- Unterstuetzt Drill Dozer &",
    "  EZ-Flash DE Rumble-Motor.",
    "- Game-Code V49E aktiviert",
    "  automatische Erkennung.",
    "- GPIO-Pin 3 im ROM-Bereich",
    "  steuert den ERM-Motor.",
    "",
    "[ HAPTISCHE TESTSTUFEN ]",
    "- 4 Dauer-Stufen waehlbar:",
    "  AUS, Puls, Flatter, Max.",
    "- Taste A: Vibrations-Impuls.",
    "- Taste L: Einzelner Klick.",
    "- Prueft Motorlager und",
    "  Treiber-Transistor.",
    "",
    "[ SEIKO ECHTZEITUHR (RTC) ]",
    "- S-35180A RTC ueber GPIO.",
    "- Prueft 32.768 kHz Quarz.",
    "- NICHT ERKANNT: 3V CR1220",
    "  Knopfzelle tauschen.",
    "- Noetig fuer Pokemon-Uhrzeit."
};
#define EZFLASH_DE_COUNT (sizeof(ezflash_lines_de) / sizeof(ezflash_lines_de[0]))

static const char* const ezflash_lines_en[] = {
    "[ EZ-FLASH DE & RUMBLE ]",
    "- Drill Dozer & EZ-Flash DE",
    "  hardware rumble support.",
    "- Game code V49E enables",
    "  rumble auto-detection.",
    "- GPIO Pin 3 in ROM space",
    "  controls the ERM motor.",
    "",
    "[ HAPTIC TEST MODES ]",
    "- 4 Continuous levels:",
    "  OFF, Pulse, Flutter, Max.",
    "- Press A for burst rumble.",
    "- Press L for single tap.",
    "- Tests motor bearings and",
    "  drive transistor health.",
    "",
    "[ SEIKO RTC REAL-TIME CLOCK ]",
    "- S-35180A RTC on GPIO.",
    "- Live clock verifies the",
    "  32.768 kHz crystal.",
    "- If NOT DETECTED: Replace",
    "  the CR1220 3V coin cell.",
    "- Required for Pokemon time."
};
#define EZFLASH_EN_COUNT (sizeof(ezflash_lines_en) / sizeof(ezflash_lines_en[0]))

typedef struct {
    const char* const* lines;
    int count;
} TopicInfo;

static const TopicInfo topics[LANG_COUNT][HELP_TOPIC_COUNT] = {
    /* LANG_DE */
    {
        { health_lines_de,  HEALTH_DE_COUNT },
        { sys_lines_de,     SYS_DE_COUNT },
        { pad_lines_de,     PAD_DE_COUNT },
        { disp_lines_de,    DISP_DE_COUNT },
        { audio_lines_de,   AUDIO_DE_COUNT },
        { bench_lines_de,   BENCH_DE_COUNT },
        { stress_lines_de,  STRESS_DE_COUNT },
        { battery_lines_de, BATTERY_DE_COUNT },
        { link_lines_de,    LINK_DE_COUNT },
        { ezflash_lines_de, EZFLASH_DE_COUNT }
    },
    /* LANG_EN */
    {
        { health_lines_en,  HEALTH_EN_COUNT },
        { sys_lines_en,     SYS_EN_COUNT },
        { pad_lines_en,     PAD_EN_COUNT },
        { disp_lines_en,    DISP_EN_COUNT },
        { audio_lines_en,   AUDIO_EN_COUNT },
        { bench_lines_en,   BENCH_EN_COUNT },
        { stress_lines_en,  STRESS_EN_COUNT },
        { battery_lines_en, BATTERY_EN_COUNT },
        { link_lines_en,    LINK_EN_COUNT },
        { ezflash_lines_en, EZFLASH_EN_COUNT }
    }
};

#define VISIBLE_LINES 12

static void draw_scroll_content(int topic, int scroll_offset) {
    const TopicInfo* t = &topics[g_lang][topic];

    /* Clear ENTIRE interior of card (text + scrollbar track) to prevent ghosting */
    gfx_draw_rect(5, 27, 230, 118, COLOR_CARD_BG);

    for (int i = 0; i < VISIBLE_LINES; i++) {
        int line_idx = scroll_offset + i;
        if (line_idx >= t->count) break;

        const char* text = t->lines[line_idx];
        if (!text || text[0] == '\0') continue;

        int y = 29 + i * 9;
        u16 col = COLOR_LIGHT_GRAY;

        if (text[0] == '[') {
            col = COLOR_CYAN_GLOW;
        } else if (text[0] == '-') {
            col = COLOR_WHITE;
        } else if (text[0] == '!') {
            col = COLOR_AMBER;
        }

        /* Clipped at x=226 to guarantee safe clearance before scrollbar */
        gfx_draw_text_clipped(7, y, 226, text, col, COLOR_CARD_BG, false);
    }

    /* Vertical scrollbar track on right (x=231, width=2) */
    int sb_x = 231;
    int sb_y = 29;
    int sb_h = 112;
    gfx_draw_line_v(sb_x, sb_y, sb_h, COLOR_DARK_GRAY);

    int max_scroll = (t->count > VISIBLE_LINES) ? (t->count - VISIBLE_LINES) : 0;
    if (max_scroll > 0) {
        int thumb_h = (VISIBLE_LINES * sb_h) / t->count;
        if (thumb_h < 10) thumb_h = 10;
        int thumb_y = sb_y + ((sb_h - thumb_h) * scroll_offset) / max_scroll;
        gfx_draw_rect(sb_x - 1, thumb_y, 3, thumb_h, COLOR_CYAN);
    }
}

void run_help_guide(enum HelpTopic initial_topic) {
    int topic = (int)initial_topic;
    if (topic < 0 || topic >= HELP_TOPIC_COUNT) topic = 0;

    int scroll_offset = 0;
    int prev_topic = -1;
    int prev_scroll = -1;
    Language prev_lang = g_lang;
    int hold_timer = 0;

    gfx_clear(COLOR_NAVY);

    while (1) {
        vsync();
        input_poll();

        if (key_hit(KEY_B) || (key_down(KEY_SELECT) && key_hit(KEY_START))) {
            break;
        }

        /* SELECT key: Live in-guide language toggle with pleasant chime */
        if (key_hit(KEY_SELECT)) {
            sound_play_square2(2000, 1, PAN_CENTER);
            lang_toggle();
            scroll_offset = 0;
            prev_topic = -1;
            prev_scroll = -1;
            continue;
        }

        /* L and R switch between topics */
        if (key_hit(KEY_R)) {
            topic = (topic + 1) % HELP_TOPIC_COUNT;
            scroll_offset = 0;
        }
        if (key_hit(KEY_L)) {
            topic = (topic - 1 + HELP_TOPIC_COUNT) % HELP_TOPIC_COUNT;
            scroll_offset = 0;
        }

        const TopicInfo* t = &topics[g_lang][topic];
        int max_scroll = (t->count > VISIBLE_LINES) ? (t->count - VISIBLE_LINES) : 0;

        /* Scrolling with UP/DOWN with smooth key repeat */
        if (key_hit(KEY_UP)) {
            if (scroll_offset > 0) scroll_offset--;
            hold_timer = 0;
        } else if (key_down(KEY_UP)) {
            hold_timer++;
            if (hold_timer > 14 && (hold_timer & 1) == 0) {
                if (scroll_offset > 0) scroll_offset--;
            }
        } else if (key_hit(KEY_DOWN)) {
            if (scroll_offset < max_scroll) scroll_offset++;
            hold_timer = 0;
        } else if (key_down(KEY_DOWN)) {
            hold_timer++;
            if (hold_timer > 14 && (hold_timer & 1) == 0) {
                if (scroll_offset < max_scroll) scroll_offset++;
            }
        } else {
            hold_timer = 0;
        }

        /* Topic or Language changed: redraw card frame, header, footer */
        if (topic != prev_topic || g_lang != prev_lang) {
            prev_topic = topic;
            prev_lang = g_lang;
            prev_scroll = -1;

            gfx_clear(COLOR_NAVY);
            gfx_draw_header(guide_headers[g_lang], topic_subtitles[g_lang][topic]);
            gfx_draw_footer(guide_footers[g_lang]);
            gfx_draw_card(4, 16, 232, 130, topic_titles[g_lang][topic], COLOR_CARD_BORDER);
        }

        /* Redraw text only when scroll position changes (zero flicker) */
        if (scroll_offset != prev_scroll) {
            prev_scroll = scroll_offset;
            draw_scroll_content(topic, scroll_offset);
        }
    }
}
