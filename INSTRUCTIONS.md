# CHAINED — User Manual & Step-by-Step Instructions 📖⚙️
### *By THE EDGE OF FEAR*

---

## 1. System Requirements & Audio Interface Setup

### Prerequisites:
- **Operating System**: Windows 10 or Windows 11 (64-bit).
- **Audio Driver**: Dedicated **ASIO Audio Interface** (e.g. Focusrite Scarlett, Universal Audio, MOTU, Behringer, Steinberg, Audient) or **ASIO4ALL / FL Studio ASIO**.

### Configuring Audio Devices:
1. Launch **`CHAINED.exe`** (or execute `RUN_CHAINED.bat`).
2. Click the **`AUDIO I/O`** button in the top navigation bar.
3. In the Audio Settings window:
   - Set **Audio device type** to **`ASIO`**.
   - Set **Device** to your soundcard's ASIO driver (e.g. *Focusrite USB ASIO*).
   - Set **Sample Rate** to **`44100 Hz`** or **`48000 Hz`**.
   - Set **Audio buffer size** to **`64`**, **`128`**, or **`256 samples`** for ultra-low latency.
   - Ensure your guitar input is enabled under **Active input channels** (Input 1 and/or Input 2) and your monitors/headphones are active under **Active output channels**.
4. Close the settings window.

---

## 2. Scanning & Loading VST3 Plugins

![CHAINED 12-Slot Daisy Chain](background%20image/CHAINED%2012.png)

### Loading a Plugin into a Slot:
1. Click the **`+ LOAD VST3`** button in any available pedal slot (Slots 1 to 12).
2. Choose from:
   - **Categorized Menu**: Select installed plugins by manufacturer (Neural DSP, STL Tones, Line 6, Waves, etc.) or effect type (Distortion, Dynamics, Reverb, etc.).
   - **`Load .vst3 from file...`**: Manually browse to any `.vst3` bundle on your hard drive.
   - **`Scan / Refresh All VST3 Plugins...`**: Automatically scans the standard Windows VST3 directory (`C:\Program Files\Common Files\VST3`).

### Interacting with Loaded Plugins:
- **`GUI` Button**: Opens/closes the floating graphical editor window of the hosted VST plugin.
- **`X` Button**: Unloads the plugin and clears the slot.
- **`ENGAGED / BYPASS` Footswitch**: Toggles the plugin on or off with realistic visual stomp LED feedback.
- **Right-Click on Slot**: Opens the slot menu for quick unloading, GUI opening, or parameter MIDI mapping.

---

## 3. Operating Modes: Unified vs. Duality

Use the top-left mode selector buttons to switch between signal routing architectures:

### Mode A: `UNIFIED [12 CHAIN]`
- Links all 12 slots into one continuous serial daisy-chain.
- Use the **`INPUT 1 (L)` / `INPUT 2 (R)` / `INPUT 1+2 (STEREO)`** dropdown on the left to set global guitar input.
- Patch cables dynamically show the signal path across all 12 slots.

### Mode B: `DUALITY [DUAL 6x6]`
- Splits the matrix into two discrete parallel chains:
  - **Row 1**: Slots 1 through 6.
  - **Row 2**: Slots 7 through 12.
- Set independent soundcard inputs for each row (e.g. Row 1 on Guitar Input 1, Row 2 on Bass/Guitar Input 2).
- Use the large **`ROW 1`** and **`ROW 2`** footswitches on the left to mute or unmute either signal path instantly.

![CHAINED Duality Parallel Rows](background%20image/CHAINED%202%20X%206.png)

---

## 4. Master DSP Controls

The top master control panel shapes your overall sound and safeguards your speakers:

- **`INPUT GAIN`**: Calibrate your dry instrument level (-24 dB to +24 dB) using the input LED VU meter.
- **`NOISE GATE`**: Adjust threshold (-90 dB to 0 dB) to clamp high-gain hiss and hum.
- **`TUBE COMP`**: Stepped rotary dial:
  - **`OFF`**: Transparent bypass.
  - **`4:1`**: Soft opto compression for cleans.
  - **`8:1`**: Studio punch and body.
  - **`12:1`**: Tight rhythm crunch.
  - **`20:1`**: Searing lead sustain and brickwall limiting.
- **`LIMITER`**: Master brickwall peak protection (-18 dB to 0 dB) clamping cleanly at -0.1 dBFS.
- **`OUTPUT GAIN`**: Sets master monitoring volume (-24 dB to +24 dB) with stereo output VU metering.

---

## 5. Backing Track Player & Practice Looper

The built-in audio player located at the bottom of the screen makes practicing effortless:

1. **Loading Audio**:
   - Click **`LOAD MP3`** to select an individual audio file (`.mp3`, `.wav`, `.aif`, `.flac`, `.ogg`).
   - Click **`LOAD FOLDER`** to import an entire album or playlist of backing tracks.
2. **Transport Controls**:
   - **`PLAY / PAUSE`**: Starts or pauses playback.
   - **`SPACE BAR`**: Press Space Bar anywhere in CHAINED to toggle Play / Pause instantly.
   - **`<<` and `>>`**: Previous and Next track buttons.
   - **`< 5s` and `5s >`**: Rewind or skip forward by 5 seconds.
   - **`AUTO-NEXT`**: Automatically advances to the next song in the folder when a track finishes.
3. **Tempo & BPM Sync**:
   - CHAINED automatically detects and displays the track's BPM upon loading.
   - Click **`BPM LINK`** to automatically set the host tempo and sync all VST delay times to the song!
4. **Time-Stretching (Speed Control)**:
   - Adjust the **`SPEED`** slider from **`0.25x`** to **`2.0x`** to slow down fast guitar solos without changing the pitch.
5. **A/B Looper**:
   - Toggle **`LOOP`** on to loop a section continuously.
   - Drag the blue start and end markers directly on the waveform to isolate any riff or solo section.

---

## 6. MIDI Footswitch Mapping & MIDI Learn

CHAINED gives you complete hardware control using standard USB MIDI foot controllers (e.g. Behringer FCB1010, Line 6 FBV, Morningstar, MeloAudio, Boss, etc.):

### Method A: Right-Click Footswitch Learn (Fastest)
1. Right-click directly on the bottom footswitch of any slot.
2. Click **`MIDI Learn (Step / Press Foot Controller)...`**.
3. The footswitch will pulse in gold with `"LEARNING MIDI..."`.
4. Press or step on your hardware foot pedal switch — the footswitch binds instantly and displays its new CC tag (e.g. `[CC 80]`)!

### Method B: Direct MIDI CC Assignment
1. Right-click any slot footswitch -> **`Direct Assign MIDI CC`**.
2. Select your desired CC number (e.g. CC #80 for Slot 1, CC #81 for Slot 2, CC #64 for Sustain/Stomp).

### Method C: Centralized MIDI Mappings Window
1. Click **`MIDI MAPPINGS`** in the top navigation bar.
2. View all 12 Slots, Row Footswitches, Mode Switch, Master Knobs, and MP3 Transport controls.
3. Click **`LEARN`** next to any target and trigger your controller.
4. Click **`AUTO-MAP SLOTS (CC 80-91)`** to map all 12 slots to CC 80 through CC 91 in one click!

---

## 7. Saving and Loading Rig Presets

- **`SAVE RIG`**: Exports your entire rig configuration—all 12 loaded VST3 plugins, their internal knob states, footswitch bypasses, master settings, and MIDI bindings—into a `.chained` XML file.
- **`LOAD RIG`**: Instantly restores your saved rig with zero configuration required.

---

*Stay loud and heavy! — **THE EDGE OF FEAR***
