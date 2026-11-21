# GBA Hardware Doctor (`GBA HW CHECK`) — v1.0.0

Bare-metal Game Boy Advance hardware diagnostic, display/IPS calibration, audio/MIDI, battery endurance, and 1-cartridge MultiBoot test suite.

## Release Assets
- **`gba_hw_check.gba`** — Ready-to-run GBA ROM (`596,736 bytes` / `582.8 KB`, Game Code `V49E`, BIOS Complement `0xE1`) for EZ-Flash Omega / Definitive Edition, EverDrive-GBA, genuine GBA/SP/Micro/DS Slot-2 hardware, and mGBA.
  - **SHA-256**: `41EEE98F7C6FE1F1CE41DF03365CBA4CBD0BE476EAF5D2EEA68773F29E8F1F1B`

## Highlights
- **10 Interactive Diagnostic Modules** with bilingual **German (DE) / English (EN)** UI (`L`/`R` in main menu, `SELECT` in the Reference Guide) and built-in 10-topic Hardware Reference Guide.
- **Display & IPS/OLED Test Suite (9 Submodes)**:
  - Dead Pixel Checker (8 colors + fullscreen OSD toggle)
  - 15-Bit RGB & Grayscale Gradient Ramps
  - Flicker-free 60 FPS Ghosting & Response Time Test (incremental edge blitting)
  - Geometry & Lens Alignment Grid with unobstructed 4-sided borders & OSD toggle
  - VCOM / Bias Flicker Calibration (1×1 / 2×2 + fullscreen)
  - IPS & OLED Shadow/Highlight Calibration + 1px Scanline/Stripe Integer Scaling Grid
  - 59.73 Hz Hardware Frame-Rate & IPS Scaler Micro-Stutter Sync Test
  - 32-Bit DMA3 VBlank-Synced VSync Tearing Scroll Test (Vertical / Horizontal / Diagonal)
  - 30 Hz Drop Shadow & Overdrive Test (static + moving boxes)
- **Audio, DirectSound & 6-Channel MIDI Sequencer**:
  - DMG Channels 1–4, 16 kHz DirectSound 8-bit PCM, Stereo Separation, Chromatic Scale
  - Glitch-free 50 Hz – 10 kHz Speaker Resonance & Frequency Sweep with live oscilloscope
  - 12-song 6-channel MIDI Sequencer with waterfall/piano visualizer and persistent SRAM presets
- **Hardware Health Audit, System Info, Gamepad/Pivot Check, Memory/CPU Benchmarks, Power-Rail Stress Test, Battery Endurance Timer, Link Cable + 1-Cartridge MultiBoot (~81 KB EWRAM client), and EZ-Flash Omega DE / Drill Dozer Hardware Rumble**.

## Key Fixes Included in v1.0.0
- Fixed `gfx_draw_text_fmt()` to fully support left-alignment (`-`), width, precision (`.N`), and unsigned `%u` formatting across all modules.
- Eliminated SRAM collision between MIDI Sequencer presets (`0x0E000000`) and Battery Endurance records (moved to `0x0E007000` with safe probe at `0x0E007FFF`).
- Completely overhauled Display & IPS/OLED submodes (DMA3 VBlank-synced Tearing Scroll, flicker-free 59.73 Hz Sync & 30 Hz Drop Shadow, fixed bounce/speed logic, fixed all text/border clippings).
- Fixed Power-Rail Stress Test double-`vsync()` (restored true 60 FPS load & accurate cycle timing) and eliminated 60 Hz re-trigger buzz in Audio Resonance Sweep.
- Fixed missing `CPU ALU:` label in Benchmarks, D-Pad Left alignment in Gamepad Test, flicker in EZ-Flash Burst & MultiBoot Slave monitor, and updated all MultiBoot payload size references.

## Quick Start
1. Copy `gba_hw_check.gba` onto your flashcart micro-SD card (or load in mGBA).
2. Boot on your GBA / GBA SP / Game Boy Micro / Nintendo DS (Slot-2).
3. Press **`SELECT`** in the menu or any module for the built-in Hardware Reference Guide.
