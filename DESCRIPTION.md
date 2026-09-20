# CHAINED — Architecture & DSP Technical Description ⛓️⚡
### *Standalone VST3 Host & Daisy-Chain Pedalboard Matrix by THE EDGE OF FEAR*

---

## 1. System Overview

**CHAINED** is a native 64-bit Windows standalone host built on the JUCE framework, specifically optimized for real-time electric guitar and bass processing. It eliminates the CPU overhead, complex routing, and latency of traditional DAWs when practicing, rehearsing, or performing live.

![CHAINED Unified Daisy Chain](background%20image/CHAINED%2012.png)

---

## 2. Signal Routing Architectures

CHAINED offers two distinct operating modes switchable on the fly with seamless buffer transitions:

```
[ UNIFIED MODE: 12-SLOT SERIAL DAISY-CHAIN ]
Input (1/2/Stereo) -> Input Gain -> Noise Gate -> [Slot 1] -> [Slot 2] -> ... -> [Slot 12] -> Backing Track -> Tube Comp -> Master Limiter -> Output Gain -> DAC

[ DUALITY MODE: DUAL PARALLEL 6x6 SIGNAL CHAINS ]
Input 1/2/Stereo -> Row 1 In -> Gate -> [Slot 1]..[Slot 6]  --\
                                                               +-> Mix -> Backing Track -> Tube Comp -> Limiter -> Output -> DAC
Input 1/2/Stereo -> Row 2 In -> Gate -> [Slot 7]..[Slot 12] --/
```

### A. Unified Mode (`UNIFIED [12 CHAIN]`)
- **12 Contiguous Pedal Slots**: Audio feeds serially from Slot 1 through Slot 12.
- **Visual Patch Cables**: Bezier-curved patch cables dynamically link active adjacent pedals with custom color gradients matching slot bypass status.
- **Row Mute Footswitches**: In Unified mode, the **ROW 1** and **ROW 2** master footswitches allow bypassing sub-blocks (Slots 1–6 or Slots 7–12) simultaneously.

### B. Duality Mode (`DUALITY [DUAL 6x6]`)
- **Dual Discrete Signal Chains**:
  - **Row 1**: Slots 1 through 6.
  - **Row 2**: Slots 7 through 12.
- **Independent Hardware Input Mapping**:
  - Row 1 can receive Soundcard Input 1 (e.g. Lead Guitar).
  - Row 2 can receive Soundcard Input 2 (e.g. Rhythm Guitar or Bass).
  - Either row can be set to Stereo 1+2.
- **Parallel Summing & Smart Zero-Bleed**:
  - Rows sum cleanly into the master bus with denormal protection.
  - If a row is enabled but contains no loaded plugins, the host silences that row's raw DI audio to prevent direct-input bleed and comb filtering.

---

## 3. Master DSP & Dynamics Section

The top master controls provide analog-modeled dynamic protection and tonal polish:

```
+-----------------------------------------------------------------------------------------------+
| [INPUT GAIN]     [NOISE GATE]     [TUBE COMP]         [LIMITER]         [OUTPUT GAIN]         |
|  -24 to +24 dB    -90 to 0 dB      OFF,4,8,12,20       -18 to 0 dB       -24 to +24 dB        |
|  (Stereo VU)                                                             (True-Peak Clamped)  |
+-----------------------------------------------------------------------------------------------+
```

### 1. Studio Noise Gate (`NOISE GATE`)
- Operates on pre-FX instrument transients with an ultra-fast attack time (sub-millisecond) and an exponential release curve.
- Range: **`-90.0 dB`** (Bypassed) to **`0.0 dB`**.
- Eliminates pickup hum, interference, and high-gain amp noise during silence while keeping sustain natural.

### 2. Stepped Optical Tube Compressor (`TUBE COMP`)
- Implements analog optical compressor ballistics with program-dependent release and subtle 2nd-harmonic tube warming:
  - **`OFF`**: 100% bit-exact DSP bypass.
  - **`4:1`**: Soft optical smoothing, ideal for cleans and dynamic acoustic tones.
  - **`8:1`**: Classic studio guitar glue and punch.
  - **`12:1`**: Tight, aggressive compression for modern drop-tuned metal rhythms.
  - **`20:1`**: Brickwall optical limiting and endless lead sustain.
- Features auto-makeup gain calculation to maintain consistent perceived volume across ratios.

### 3. Master True-Peak Brickwall Limiter (`LIMITER`)
- Sits immediately before the output gain stage with a **`-0.1 dBFS`** hard true-peak ceiling.
- Threshold adjustable from **`-18.0 dB`** to **`0.0 dB`**.
- Features a smooth soft-knee curve with a 1 ms attack and 40 ms release, completely eliminating digital inter-sample DAC clipping and headphone crackle when layering multi-amp rigs with backing tracks.

### 4. Floating-Point Denormal Protection
- Enforces `juce::ScopedNoDenormals` across the real-time audio thread, preventing CPU spikes and quantization noise caused by high-gain amp simulations and cab impulse responses processing low-level audio tails.

---

## 4. Multi-Slot MIDI Architecture

Traditional VST hosts feed MIDI serially through plugins, which causes the first plugin in the chain to consume and clear the MIDI buffer—leaving downstream plugins starved of MIDI.

CHAINED features a custom **Concurrent Multi-Slot MIDI Dispatcher**:

```
MIDI In ---> [ ChainedMidiManager ] ---> Host Target Callbacks (Footswitches, Gain, Transport)
                     |
                     +---> Slot 1  (Clean MidiBuffer Copy)
                     +---> Slot 2  (Clean MidiBuffer Copy)
                     +---> ...
                     +---> Slot 12 (Clean MidiBuffer Copy)
```

- **Thread-Safe Architecture**: Employs `std::recursive_mutex` and decoupled internal operations to guarantee zero deadlocks and zero audio hangs during live MIDI Learn.
- **Independent Plugin MIDI**: Every hosted plugin receives a clean, unconsumed copy of all incoming MIDI events, allowing multi-plugin setups with internal MIDI sync (e.g. Archetype Gojira wah/pitch pedals, Neural DSP preset changes, delay taps).
- **Direct CC Assignment**: Right-click any footswitch for 1-click binding to any MIDI CC (0–127) or MIDI Note.

---

## 5. Backing Track Looper & Transient BPM Engine

The built-in audio player integrates directly with host tempo and VST delay synchronization:

- **Format Decoders**: Native support for MP3, WAV, AIFF, FLAC, and OGG formats.
- **Onset Detection & BPM Estimation**: Automatically scans the audio waveform upon loading, extracting transient energy peaks to compute track tempo.
- **BPM Link**: One click aligns CHAINED's master tempo to the detected song BPM, syncing all loaded VST delays, tremolos, and arpeggiators.
- **Time-Stretch Engine**: Independent speed adjustment from **`0.25x`** to **`2.0x`** with locked pitch preservation.
- **Visual Waveform Looper**: Interactive waveform with draggable loop start and end boundaries for practicing difficult guitar solos and riffs.
- **Keyboard Shortcuts**: Window-level **Space Bar** transport control for instantaneous Play/Pause/Stop.

---

## 6. Rig Preset System (`.chained`)

Preset state is serialized into structured XML files:
- All 12 plugin binaries, unique IDs, formats, and internal VST state byte chunks.
- Slot bypass states and footswitch configurations.
- Input routing modes and gain staging.
- Master Noise Gate, Tube Compressor, Limiter, and Tempo settings.
- All active MIDI Controller and Footswitch mappings.

---

*Designed and engineered for high-gain precision by **THE EDGE OF FEAR**.*
