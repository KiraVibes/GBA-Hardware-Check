# GBA Hardware Doctor (GBA HW CHECK)

A comprehensive bare-metal diagnostic, calibration, and hardware testing suite for the **Game Boy Advance**, written in freestanding ARM7TDMI C with zero external SDK dependencies.

Optimized for genuine hardware (**AGB-001**, **AGS-001**, **AGS-101**, **Game Boy Micro**, **Game Boy Player**, **Nintendo DS / DS Lite** in Slot-2 GBA mode), aftermarket **IPS / OLED screen mods**, flashcarts (**EZ-Flash Omega**, **EZ-Flash Omega Definitive Edition**, **EverDrive-GBA**), **1-Cartridge MultiBoot** over Link Cable, and emulators (**mGBA**).

---

## Features & Diagnostic Modules (10 Modules, Bilingual DE / EN)

Press **`L` / `R`** in the main menu (or **`SELECT`** inside the Reference Guide) to switch seamlessly between **German (DE)** and **English (EN)**.

### 1. Hardware Health & Audit Score
- One-screen automated hardware sweep across **CPU/ALU**, **32 KB IWRAM**, **256 KB EWRAM**, **96 KB VRAM**, **Sound subsystem**, **SIO/Link Port**, **Save SRAM / Cartridge RTC**, and **Keypad sanity**.
- Live stuck-button and D-Pad pivot short-circuit detection.
- Calculates a **0–100% Console Health Score** and certification grade (`A+`, `A`, `B`, `C`).

### 2. System & Console Diagnostics
- **Hardware Detection**: Distinguishes genuine GBA hardware (AGB/AGS/Micro) from **Nintendo DS / DS Lite** via `REG_WRAMCNT` behavior (and safely detects MultiBoot EWRAM execution).
- **Bus Timings (`REG_WAITCNT`)**: Reads live SRAM and ROM WS0 First/Second waitstates and Prefetch Buffer state.
- **Non-Destructive SRAM/FRAM Probe**: Tests save memory safely at the top unused byte (`0x0E007FFF`).
- **Live Seiko S-3511A / S-35180A RTC Clock**: Communicates over cartridge GPIO (`0x080000C4..0x080000C8`) to verify the 32.768 kHz quartz crystal and CR1220 coin cell.

### 3. Gamepad & Button Hardware Check
- Real-time visual controller layout for all 10 inputs (`A`, `B`, `L`, `R`, `START`, `SELECT`, `UP`, `DOWN`, `LEFT`, `RIGHT`).
- **Actuation Counters**: Tracks button presses to expose switch bounce or missed contacts.
- **D-Pad Pivot Integrity Check**: Flags `!PIVOT ERR!` if opposite directions (`UP+DOWN` or `LEFT+RIGHT`) register simultaneously.
- **4-Way Diagonal Test** (`UL`, `UR`, `DL`, `DR`) and **Soft-Reset Combo** (`A+B+START+SELECT`). Hold **`B`** to exit.

### 4. Display Quality & IPS/OLED Test Suite (9 Submodes)
Switch between submodes using **`L` / `R`**:
1. **Dead Pixel Checker**: 8 solid colors (White, Black, Red, Green, Blue, Yellow, Cyan, Magenta) with **`SELECT`** fullscreen OSD toggle.
2. **15-Bit RGB & Grayscale Ramps**: 32-step Red, Green, Blue, and Grayscale gradients to check gamma and color banding.
3. **Flicker-Free Ghosting & Response Test**: 60 FPS animated contrast bars with selectable speed (1–8 px/frame) using incremental edge blitting.
4. **Geometry & Lens Alignment Grid**: 16×16 grid, center crosshair, and double 1px outer alignment borders with **`SELECT`** OSD hide toggle.
5. **VCOM / Bias Flicker Calibration**: 1×1 and 2×2 checkerboard patterns with **`SELECT`** edge-to-edge fullscreen mode for motherboard VCOM pot adjustment.
6. **IPS & OLED Calibration**: Near-black shadow detail (`RGB 0..5`) and highlight clipping (`RGB 26..31`) swatches, plus a 1px horizontal scanline / vertical stripe integer-scaling grid (**`A`** to toggle, **`SELECT`** to hide OSD).
7. **59.73 Hz Frame-Rate & IPS Scaler Sync**: Hardware `TM0+TM1` 32-bit cycle counter verifying exact `59.728 Hz` (`280,896 cycles/frame`) VBlank timing alongside a smooth 60 FPS gliding track to visually expose periodic 60 Hz IPS scaler micro-stutter.
8. **VSync Tearing Scroll Test**: 32-bit DMA3 VBlank-synced high-speed vertical, horizontal, and diagonal raster patterns (1–16 px/frame, **`A`** pattern, **`START`** pause).
9. **30 Hz Drop Shadow & Overdrive Test**: Clean 30 Hz static and moving drop-shadow boxes over striped backgrounds (`A` pause, `LEFT`/`RIGHT` speed).

### 5. Sound, DirectSound & MIDI Sequencer Suite
- **DMG Channels 1–4**: Square 1 Pitch Sweep, Square 2 440 Hz Lead, 4-bit Wave RAM Sine Synth, and LFSR Noise Drum.
- **DirectSound 8-Bit PCM (DMA1)**: Hardware FIFO PCM audio playback at 16 kHz.
- **Stereo Separation Test** (Left / Right / Center) & **13-Note Chromatic Scale**.
- **Speaker Resonance & Frequency Sweep**: Glitch-free 50 Hz – 10,000 Hz manual/auto sweep (Square 2 & Wave RAM Sine) with live oscilloscope and resonance diagnosis guide.
- **6-Channel MIDI Sequencer & Piano Visualizer** (Cartridge ROM): 12 built-in songs, 6 live channels (`CH1–CH4` + `DSA`/`DSB` 24 PCM instruments), real-time waterfall & piano visualizer, per-channel Inspector (Track assignment, Tone, Octave, Volume, Pan, Mute/Solo), and **3 persistent SRAM preset slots per song**.

### 6. Memory & CPU Benchmarks
- Measures **16 KB EWRAM Write/Read** and **ROM/IWRAM Read** bandwidth in cycles and MB/s.
- **CPU ALU Benchmark**: 10,000 32-bit math operations with bus classification.
- **Frame Oscillator Timing**: Verifies nominal `280,896 cycles` (`59.7275 Hz`).
- **RAM Data Bus Integrity**: Walking 1s and alternating `0xAA55` / `0x55AA` patterns across all 16 data lines.

### 7. Power-Rail & Capacitor Stress Test
- Sustains true 60 FPS **100% CPU 3D wireframe**, EWRAM bus burst, and simultaneous 4-channel audio load to stress the DC-DC regulator, power switch, and electrolytic capacitors (`C35–C37` / `C33`).
- Real-time cycle jitter and dropped-frame monitor (`A` toggles audio load, `START` resets stats).

### 8. Battery Endurance & Run-Down Benchmark
- Three selectable workload profiles (**Idle**, **Normal Gameplay**, **Max 3D Cube Load**).
- Hardware `TM2+TM3` 16.384 kHz crystal timer (`1.000 s` exact ticks) + optional Cartridge RTC wall-clock start timestamp.
- Auto-saves progress every second to upper SRAM (`0x0E007000`, separate from MIDI presets at `0x0E000000`) so runtime survives battery depletion.

### 9. Link Cable, SIO Port & 1-Cartridge MultiBoot Suite
- 16-bit Multi-Player SIO diagnostics at 115,200 baud with live Master/Slave role detection, pin telemetry (`SI`/`SD`), packet counter, and timestamped event log.
- **Built-In 1-Cartridge MultiBoot**: Detects a second cartridgeless GBA in BIOS and uploads the ~81 KB standalone diagnostic client directly into GBA 2's EWRAM (`SWI 0x25`)!

### 10. EZ-Flash Omega DE & Hardware Rumble Suite
- Uses ROM Game Code **`V49E`** (Drill Dozer) for automatic hardware rumble activation on **EZ-Flash Omega Definitive Edition** and compatible GPIO Pin 3 rumble cartridges.
- 4 continuous rumble modes (`OFF`, `PULSE`, `FLUTTER`, `CONTINUOUS`), plus **`A`** burst (30 frames) and **`L`** sharp tap (8 frames), alongside live RTC battery verification.

---

## Installation (Flashcart / Emulator)

1. Download **`gba_hw_check.gba`** from the [Releases](../../releases) page.
2. Copy `gba_hw_check.gba` to your micro-SD card (e.g., EZ-Flash Omega / Definitive Edition or EverDrive-GBA) or open it in **mGBA**.
3. On EZ-Flash Omega, launch with **Clean Boot** (or normal boot). SRAM saves (`SRAM_V113`) and RTC (`SIIRTC_V001`) are detected automatically.

---

## Building from Source

The project builds bare-metal using **xPack ARM GCC (`arm-none-eabi-gcc 14.2.1-1.1`)** — no devkitPro installation required.

### 1. Fetch the Portable Toolchain (first time only)
```bash
python setup_toolchain.py
```
*(Or ensure `arm-none-eabi-gcc` and `arm-none-eabi-objcopy` are on your `PATH`.)*

### 2. Build the ROM & MultiBoot Client Payload
- **Python (cross-platform):**
  ```bash
  python build.py
  ```
- **Windows Double-Click / PowerShell:**
  Run `build.bat` or `powershell -ExecutionPolicy Bypass -File .\build.ps1`.

The build automatically:
1. Compiles the standalone EWRAM MultiBoot client (`multiboot_client/mb_main.c`) and regenerates `include/multiboot_payload.h`.
2. Assembles `src/crt0.s` and compiles all C modules (`-mcpu=arm7tdmi -O2 -Wall -Wextra`).
3. Links `build/gba_hw_check.elf` via `gba.ld` and extracts the raw binary.
4. Patches the official 156-byte Nintendo Logo, title (`GBA HW CHECK`), game code (`V49E`), and BIOS complement checksum at `0xBD`.

---

## License & Disclaimer

- **Source Code License:** The source code and build scripts of this project are licensed under the **MIT License** — see [LICENSE](LICENSE) for details.
- **MIDI Test Sequences:** The MIDI arrangements in `include/midi_data.h` are excluded from the MIT License. They are included strictly for non-commercial hardware and audio testing purposes and remain the intellectual property of their respective copyright holders.
- **Trademarks:** *Game Boy Advance*, *Game Boy Micro*, *Game Boy Player*, and *Nintendo DS* are trademarks of Nintendo Co., Ltd. *EZ-Flash* and *EverDrive* are trademarks of their respective owners. This project is an independent homebrew diagnostic tool and is not affiliated with or endorsed by Nintendo.

