# CHAINED ⛓️🎸 — Standalone VST3 Host & Daisy-Chain Pedalboard
### *By THE EDGE OF FEAR*

[![Platform](https://img.shields.io/badge/Platform-Windows%20x64-black.svg)]()
[![Format](https://img.shields.io/badge/Format-Standalone%20x64%20EXE-red.svg)]()
[![Audio](https://img.shields.io/badge/Audio-ASIO%20%7C%20WASAPI%20%7C%20DirectSound-blue.svg)]()
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-green.svg)](LICENSE)

> **CHAINED** is a high-performance, low-latency standalone 64-bit VST3 host and pedalboard emulator engineered specifically for modern guitarists, bassists, and live performers. Run complex 12-pedal signal chains, dual parallel amp/FX rigs, backing tracks with pitch-locked tempo control, studio tube compression, true-peak limiting, and full MIDI footswitch control without opening a bulky DAW!

---

## 📸 Screenshots & Operating Modes

### 1. Unified Mode (12 Daisy-Chained VST3 Pedal Slots)
*A single contiguous 12-pedal daisy-chain with realistic visual patch cables linking every pedal in series.*

![CHAINED Unified 12-Slot Daisy Chain](background%20image/CHAINED%2012.png)

---

### 2. Duality Mode (Dual 2x6 Parallel Signal Chains)
*Split into two independent 6-slot parallel rows with dedicated soundcard input selection and mute/unmute footswitches.*

![CHAINED Duality 2x6 Parallel Rows](background%20image/CHAINED%202%20X%206.png)

---

## 📚 Documentation & User Guides

- 📖 **[Full Architecture & DSP Description](DESCRIPTION.md)** — In-depth breakdown of signal paths, master DSP section, MIDI engine, and audio looper.
- 🎛️ **[User Manual & Step-by-Step Instructions](INSTRUCTIONS.md)** — Guide on audio setup, loading plugins, MIDI foot controller mapping, and rig presets.

---

## ⚡ Key Highlights & Features

### 🎛️ 1. Flexible 12-Slot Daisy-Chain Routing Matrix
- **Unified Mode (`UNIFIED [12 CHAIN]`)**:
  - Connects all 12 VST3 slots in a single contiguous serial chain.
  - Realistic patch cables dynamically connect each pedal slot from Slot 1 through Slot 12.
  - Global Master Input Routing: Select **Input 1 (L)**, **Input 2 (R)**, or **Input 1+2 (Stereo)**.
- **Duality Mode (`DUALITY [DUAL 6x6]`)**:
  - Splits the matrix into two discrete parallel 6-pedal rows (**Row 1: Slots 1–6**, **Row 2: Slots 7–12**).
  - Independent hardware input selection per row (e.g. Row 1 on Guitar Input 1, Row 2 on Bass Input 2 or Synth Input 2).
  - Dedicated **Row 1** and **Row 2** Master Footswitches to instantly toggle or mute either signal path.
  - Smart zero-bleed architecture: Empty rows do not bleed raw direct input (DI) audio.

### 🛡️ 2. Studio Master DSP Chain
- **High-Gain Noise Gate (`NOISE GATE`)**: Ultra-fast attack, smooth release gate (-90 dB to 0 dB) to silence high-gain pickup hum and amp hiss.
- **Stepped Optical Tube Compressor (`TUBE COMP`)**:
  - 5 switchable modes: **`OFF`**, **`4:1`** (gentle opto warmth), **`8:1`** (guitar glue & punch), **`12:1`** (tight rhythm crunch), and **`20:1`** (all-buttons-in limiting & sustain).
  - Program-dependent optical release and subtle 2nd-harmonic tube warmth.
- **Master Brickwall Limiter (`LIMITER`)**:
  - Adjustable threshold (-18 dB to 0 dB) with true-peak soft-knee protection clamping at **-0.1 dBFS**.
  - Completely prevents digital inter-sample DAC clipping and headphone crackle.
- **Input & Output Gain Staging**: Precision -24 dB to +24 dB dials with responsive stereo LED VU meters.

### 🎵 3. Backing Track Player & Practice Looper
- **Multi-Format Support**: Plays MP3, WAV, AIFF, FLAC, and OGG files.
- **Transient BPM Auto-Detection**: Automatically detects track tempo and displays it in real time.
- **BPM Link Mode**: One-click syncs the global host BPM and all time-based VST delays/choruses to the loaded track.
- **Pitch-Locked Speed Control**: Slow down or speed up backing tracks (0.25x to 2.0x) with zero pitch shifting.
- **Visual Waveform Looper**: Interactive audio waveform with draggable A/B loop start and end markers.
- **Space Bar Transport Shortcut**: Press **Space Bar** anywhere in the app to instantly Play / Pause / Stop.
- **Auto-Next & Playlist Support**: Load entire folders and advance through tracks automatically.

### 🎚️ 4. Multi-Slot MIDI Distribution & Live MIDI Learn
- **Concurrent Multi-Slot Distribution**: Unlike standard hosts where the first plugin eats the MIDI buffer, CHAINED feeds clean independent MIDI copies to every single slot (Slots 1–12) simultaneously.
- **Footswitch MIDI Learn**: Right-click any slot footswitch to bind instantly to your hardware MIDI foot controller.
- **Dedicated MIDI Mappings Manager**: Top-bar **`MIDI MAPPINGS`** window with live learn indicators, clear buttons, and **`AUTO-MAP SLOTS (CC 80-91)`** for 12-button MIDI pedalboards.
- **Live Visual Status**: Footswitches glow in amber with `"LEARNING MIDI..."` during learn mode and display their assigned CC badge (e.g. `[CC 80]`).

### 💾 5. Rig Preset System
- Save and recall complete rig configurations with 1 click using **`SAVE RIG`** and **`LOAD RIG`** (`.chained` XML format).
- Persists all 12 plugin states, bypass configurations, input routes, master DSP knobs, BPM, and MIDI controller maps.

---

## 🚀 Quick Start (Running CHAINED Standalone)

1. Download **`CHAINED.exe`** from the repository or clone this repository.
2. Double-click **`CHAINED.exe`** or run **`RUN_CHAINED.bat`**.
3. Click **`AUDIO I/O`** in the top bar to select your **ASIO Audio Interface** (e.g. Focusrite, Universal Audio, MOTU, Audient, ASIO4ALL).
4. Click **`+ LOAD VST3`** on any slot to load your favorite amp sims, pedals, and effects (e.g. Neural DSP, STL Tones, Line 6 Helix, Amplitube, Waves, FabFilter).
5. Plug in your guitar, load a backing track, and rock out!

---

## 🎸 Support & Connect

Stay RAD Metal Heads... The Edge Of Fear  
- 📺 **YouTube**: https://www.youtube.com/@theedgeoffearmetal  
- 📸 **Instagram**: https://www.instagram.com/theedgeoffear/  
- 🔊 **SoundCloud**: https://soundcloud.com/user-290758847  

---

## 📄 License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**. See the [LICENSE](LICENSE) file for details.

---

## 💀 Credits

Designed and developed with ⚡ by **The Edge of Fear**.
