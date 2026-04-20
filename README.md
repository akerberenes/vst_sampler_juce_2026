# Sampler with Freeze - VST Plugin

A JUCE-based VST3/AU plugin and standalone sampler with an echo-freeze effect for creative audio manipulation. Designed for portability to Teensy 4.1 hardware.

---

## What This Plugin Does

Imagine a sampler rack with 4 pads. You load a sample on each pad, trigger them with MIDI, and all the audio flows through a **freeze effect** — a kind of instant loop capturer that grabs whatever just played and lets you stutter, slow down, speed up, or reshape that captured loop in real time.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         SAMPLER WITH FREEZE                            │
│                                                                        │
│  ┌─────────┐                                                          │
│  │ Sample 1 │──┐                                                      │
│  └─────────┘  │   ┌───────┐    ┌────────────────┐    ┌───────────┐   │
│  ┌─────────┐  ├──→│       │    │                │    │           │   │
│  │ Sample 2 │──┤   │ Mixer │───→│  Freeze Effect │───→│  Output   │   │
│  └─────────┘  │   │       │    │                │    │           │   │
│  ┌─────────┐  ├──→│       │    │  [capture]     │    └───────────┘   │
│  │ Sample 3 │──┤   │       │    │  [loop]        │                    │
│  └─────────┘  │   │       │    │  [stutter]     │                    │
│  ┌─────────┐  │   │       │    │  [speed]       │                    │
│  │ Sample 4 │──┘   │       │    └────────────────┘                    │
│               ┌───→│       │                                          │
│  Mic / DAW ───┘    └───────┘                                          │
│  Input         (optional,                                             │
│                 depends on mode)                                       │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## The Two Routing Modes

The **Mixer** controls what audio the Freeze Effect captures.

### Sequential Mode (default)
Everything gets mixed together — your mic/DAW input AND the samplers. The freeze captures it all.

```
  Mic/DAW Input ────┐
                    ├──→ Mixed together ──→ Freeze captures this ──→ Output
  Sampler Output ───┘
```

**Use case:** You're singing over a beat loop. Hit freeze — it captures your voice AND the beat. Now stutter both together.

### Parallel Mode
The samplers go to freeze, but your mic/DAW input stays independent. You can hear both, but only the sampler content gets frozen.

```
  Mic/DAW Input ─────────────────────────────────────────────→ Output
                                                                  ↑
  Sampler Output ──→ Freeze captures only this ──→ Frozen audio ──┘
                                                   (mixed back in)
```

**Use case:** You're playing guitar live while triggering sample loops. Hit freeze — it captures only the samples. You keep playing guitar over the frozen loop.

---

## How Each Module Works

### AudioBuffer — The Container

Think of this like an empty mixing board channel strip. It holds audio data (samples as numbers) that gets passed between modules.

```
  AudioBuffer = a grid of numbers

  Channel 0:  [ 0.1,  0.3, -0.2,  0.5, ... ]   ← left speaker
  Channel 1:  [ 0.0,  0.4, -0.1,  0.6, ... ]   ← right speaker
              ──────────────────────────────→
              time (one number per sample, 48,000 per second)
```

Nothing fancy — just a container that other modules write to and read from.

---

### Sampler — One Sample Player

Each Sampler holds one audio file and plays it back when triggered by MIDI. It knows about tempo so it can sync playback length to beats.

```
  MIDI Note On ──→ ┌─────────────────────────────┐
                   │ Sampler                      │
                   │                              │
  Audio file ────→ │  "Play this WAV for 4 beats  │ ──→ Audio out
  (loaded once)    │   at 120 BPM"                │
                   │                              │
                   │  4 beats ÷ 120 BPM × 60 sec │
                   │  = 2 seconds                 │
                   │  = 96,000 samples @ 48kHz    │
                   └─────────────────────────────┘
```

**Key behavior:**
- One-shot: one note = one playback (no stacking/polyphony per pad)
- Tempo-synced: duration is calculated in beats, not seconds
- Loop fraction: can repeat just a portion of the sample (e.g., first half)
- Linear interpolation: smooth playback even at fractional positions

---

### SamplerBank — Four Samplers Together

A container that holds 4 independent Sampler instances and sums their output. Like having 4 MPC pads.

```
  MIDI C3  ──→ ┌─────────┐
               │ Sampler 0 │──┐
  MIDI C#3 ──→ ├─────────┤  │
               │ Sampler 1 │──┤   ┌─────┐
  MIDI D3  ──→ ├─────────┤  ├──→│ SUM │──→ Combined sampler output
               │ Sampler 2 │──┤   └─────┘
  MIDI D#3 ──→ ├─────────┤  │    (each at 25% volume to prevent
               │ Sampler 3 │──┘     clipping when all 4 play)
               └─────────┘
```

---

### Mixer — The Routing Switch

Decides what audio the Freeze Effect will hear. Has two knobs (input level, sampler level) and a mode switch.

```
  ┌─────────────────────────────────────────────────┐
  │ Mixer                                           │
  │                                                 │
  │  Mode: [Sequential / Parallel]                  │
  │                                                 │
  │  SEQUENTIAL:                                    │
  │    output = input × inputLevel                  │
  │           + sampler × samplerLevel              │
  │                                                 │
  │  PARALLEL:                                      │
  │    output = sampler × samplerLevel              │
  │    (input bypasses — handled later)             │
  └─────────────────────────────────────────────────┘
```

---

### CircularBuffer — The Recording Tape Loop

This is the engine inside the Freeze Effect. Think of a tape loop that constantly records over itself. When you "freeze," the tape stops recording but keeps playing.

```
  RECORDING (normal operation):
  ┌──────────────────────────────────┐
  │ ▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░│  ← audio fills the buffer
  │ ↑ write pointer moves forward →  │     like a tape loop
  └──────────────────────────────────┘

  FROZEN (freeze engaged):
  ┌──────────────────────────────────┐
  │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│  ← buffer is full and locked
  │    ↑ read pointer loops around → │     write pointer stopped
  └──────────────────────────────────┘

  LOOP BOUNDARIES (stutter region):
  ┌──────────────────────────────────┐
  │ ░░░░░▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░│  ← only this region plays
  │      ↑ loop start  ↑ loop end   │     (shorter = faster stutter)
  └──────────────────────────────────┘
```

**Controls:**
- **Playback speed:** 1.0 = normal pitch, 0.5 = half speed / octave down, 2.0 = double speed / octave up
- **Loop position/length:** Control what region of the buffer plays (shorter = tighter stutter grains)
- **Read pointer retrigger:** The stutter mechanism — resets the playhead to loop start rhythmically

---

### FreezeEffect — The Brain

Orchestrates everything: recording, freezing, stuttering, speed, and dry/wet mixing. This is the module that makes the magic happen.

```
  Audio in ──→ ┌──────────────────────────────────────────────┐
               │ FreezeEffect                                 │
               │                                              │
               │  State: RECORDING                            │
               │  ┌──────────────────────┐                    │
               │  │ CircularBuffer       │                    │
               │  │ (continuously        │                    │
               │  │  capturing audio)    │                    │
               │  └──────────────────────┘                    │
               │                                              │
               │  [User hits FREEZE button]                   │
               │                                              │
               │  State: FROZEN                               │
               │  ┌──────────────────────┐                    │
               │  │ CircularBuffer       │                    │
               │  │ (looping captured    │ ──→ frozen audio   │
               │  │  audio, no new       │     mixed with     │
               │  │  recording)          │     dry signal     │
               │  └──────────────────────┘     via dry/wet    │
               │                                              │
               │  Stutter: retriggers read pointer at loop start on the beat  │
               │  Speed:   changes how fast the loop plays    │
               │  Dry/Wet: blend between original and frozen  │
               └──────────────────────────────────────────────┘ ──→ Output
```

**Stutter timing example at 120 BPM:**
```
  1/8 beat stutter = retriggers every 0.25 seconds (fast choppy repeat)
  1/4 beat stutter = retriggers every 0.5 seconds  (rhythmic repeat)
  1/2 beat stutter = retriggers every 1.0 second   (slow echo feel)
```

---

## Full Signal Flow

Here's how all the modules connect, from start to finish:

```
  ┌──────────────┐
  │ DAW / Mic    │ ← audio comes in from your DAW or microphone
  │ (Audio In)   │
  └──────┬───────┘
         │
         ▼
  ┌──────────────┐     ┌─────────────┐
  │              │     │ SamplerBank │ ← 4 samples triggered by MIDI
  │              │     │ (4 pads)    │
  │              │     └──────┬──────┘
  │              │            │
  │    Mixer     │◄───────────┘
  │              │
  │  Sequential: │ input + samplers combined
  │  Parallel:   │ samplers only (input goes straight to output)
  └──────┬───────┘
         │
         ▼
  ┌──────────────┐
  │ FreezeEffect │ ← captures mixed audio into circular buffer
  │              │   freeze = loop the buffer
  │  [Record]    │   stutter = rhythmic jumps
  │  [Freeze]    │   speed = pitch shift
  │  [Stutter]   │   dry/wet = blend
  │  [Speed]     │
  │  [Dry/Wet]   │
  └──────┬───────┘
         │
         ▼
  ┌──────────────┐
  │ DAW Output   │ ← final audio goes back to your DAW
  │ (+ raw input │   (in parallel mode, raw input is mixed back
  │  in parallel │    here so you can still hear it)
  │  mode)       │
  └──────────────┘
```

---

## Features

- **4-Sample One-Shot Sampler** with tempo-synced looping and per-sample loop position/length editing
- **Tabbed Waveform Editor** with draggable markers per sample
- **MIDI Triggering** — C1, C2, C3, C4 mapped to samples 1–4, with per-sample Note Off control
- **Echo Freeze Effect** on master output
- **Stutter Control** with rhythmic quantization (locked to BPM)
- **Speed Manipulation** for pitch shifting frozen audio
- **Dry/Wet Mix** with loop start/end boundary control
- **Parallel/Sequential Routing** for flexible signal flow
- **Preset System** with embedded sample storage (planned)

## Project Structure

```
vst_plugin_project/
├── src/
│   ├── dsp/              # Audio processing core (pure C++, no dependencies)
│   │   ├── AudioBuffer       — multi-channel audio container
│   │   ├── CircularBuffer    — recording tape loop for freeze
│   │   ├── Sampler           — single sample playback with tempo sync
│   │   ├── SamplerBank       — 4 sampler instances mixed together
│   │   ├── Mixer             — routing switch (sequential/parallel)
│   │   └── FreezeEffect      — freeze/stutter/speed orchestrator
│   ├── juce/             # VST plugin wrapper (JUCE framework)
│   │   ├── PluginProcessor   — connects DSP to the DAW
│   │   ├── PluginEditor      — top-level GUI layout
│   │   ├── SampleTabPanel    — tabbed per-sample editor
│   │   └── WaveformDisplay   — interactive waveform with drag markers
│   ├── audio/            # File loading (WAV, MP3, FLAC)
│   │   ├── AudioFileLoader   — reads audio files into memory
│   │   └── PresetManager     — save/load presets with embedded samples
│   └── hardware/         # Future Teensy 4.1 hardware port
├── unit_tests/           # Automated tests (numeric validation, no audio)
├── docs/                 # Architecture and porting guides
└── CMakeLists.txt        # Build configuration
```

---

## Source File Reference

Each file below is described with its purpose and where it sits in the processing chain.

### DSP Core — [`src/dsp/`](src/dsp/)

These files are pure C++17 with no external dependencies. They can be compiled for any platform (PC, Mac, Teensy 4.1).

#### [`AudioBuffer.h`](src/dsp/AudioBuffer.h) / [`.cpp`](src/dsp/AudioBuffer.cpp)

**Role in chain:** Data container passed between every module.

Simple multi-channel audio buffer. Wraps a flat `std::vector<float>` with per-channel read/write pointer access. Every DSP module reads from and writes to AudioBuffer instances (or raw `float*` arrays that serve the same purpose).

Key API: `allocate(channels, frames)`, `clear()`, `getWritePointer(ch)`, `getReadPointer(ch)`.

#### [`Sampler.h`](src/dsp/Sampler.h) / [`.cpp`](src/dsp/Sampler.cpp)

**Role in chain:** Individual sample playback engine (one per pad).

```
MIDI Note On ──→ [ Sampler ] ──→ audio samples out
```

Loads a mono audio file (as a `float` vector), and plays it back when triggered. Supports:
- **Tempo-synced duration** — beat count × BPM determines playback length.
- **Start/end fractions** — `setStartFraction()` / `setEndFraction()` define the active region (0.0–1.0). Playback begins at the start marker and stops at the end marker.
- **Loop mode** — when enabled, playback wraps from end back to start indefinitely.
- **Linear interpolation** — smooth output even at fractional sample positions.

`trigger(beatDuration, tempo)` resets the playhead to the start fraction. `processBlock(output, numSamples, tempo)` fills the output buffer with interpolated audio.

#### [`SamplerBank.h`](src/dsp/SamplerBank.h) / [`.cpp`](src/dsp/SamplerBank.cpp)

**Role in chain:** Multiplexes 4 Samplers into a single summed output.

```
Sampler 0 ──┐
Sampler 1 ──┤
Sampler 2 ──┼──→ sum (each at 25% gain) ──→ combined sampler output
Sampler 3 ──┘
```

Holds an array of 4 Sampler instances. Routes `triggerSample(index)` to the correct pad. `processBlock()` calls each sampler's processBlock and sums them. Per-sample controls: `setSampleStartFraction(i, f)`, `setSampleEndFraction(i, f)`, `setSampleLoopMode(i, b)`.

#### [`Mixer.h`](src/dsp/Mixer.h) / [`.cpp`](src/dsp/Mixer.cpp)

**Role in chain:** Routing switch between input audio and sampler audio.

```
Sequential:  output = input × inputLevel + sampler × samplerLevel
Parallel:    output = sampler × samplerLevel   (input bypasses to final output)
```

Two knobs (`setInputLevel`, `setSamplerLevel`) and a mode switch (`setMode`). In parallel mode, the mixer only passes sampler audio to the freeze effect; the DAW/mic input is mixed back in at the final output stage by the PluginProcessor.

#### [`CircularBuffer.h`](src/dsp/CircularBuffer.h) / [`.cpp`](src/dsp/CircularBuffer.cpp)

**Role in chain:** Internal engine of the FreezeEffect — the "tape loop."

A ring buffer that continuously records incoming audio. When frozen, writing stops and the read pointer loops over the captured content. Supports:
- **Playback speed** — fractional read-pointer advancement (0.5 = half speed, 2.0 = double).
- **Loop start/end** — fraction-based boundaries that define the stutter region.
- **Freeze/unfreeze** — atomic state toggle.

`pushBlock()` writes new audio in. `pullBlock()` reads audio out with interpolation.

#### [`FreezeEffect.h`](src/dsp/FreezeEffect.h) / [`.cpp`](src/dsp/FreezeEffect.cpp)

**Role in chain:** Master effect — sits between the mixer output and the final DAW output.

```
mixer output ──→ [ FreezeEffect ] ──→ final audio
```

Orchestrates the CircularBuffer plus beat-synced stutter timing. Controls:
- `setFrozen(bool)` — toggle capture vs. loop playback.
- `setStutterFraction(f)` — how often the read pointer retriggers at loop start (smaller = faster chop).
- `setPlaybackSpeed(f)` — pitch-shift the frozen loop.
- `setLoopLength(f)` — fraction of the buffer used as the loop window (1.0 = full buffer).
- `setLoopPosition(f)` — shift the window start within the buffer (clamped so end stays in bounds).

When frozen, output is always 100% wet (the frozen loop only — no dry signal blended in).

`processBlock()` does the work: pushes audio into the buffer, reads frozen output.

---

### JUCE Plugin Layer — [`src/juce/`](src/juce/)

These files use the JUCE framework and are specific to the VST3/Standalone build. They wrap the DSP core with parameter management, MIDI handling, and GUI.

#### [`PluginProcessor.h`](src/juce/PluginProcessor.h) / [`.cpp`](src/juce/PluginProcessor.cpp)

**Role in chain:** The central hub — connects everything.

```
DAW audio in ──→ PluginProcessor ──→ DAW audio out
DAW MIDI in  ──→       │
                       │
           ┌───────────┼───────────┐
           │    reads APVTS params │
           │    routes MIDI        │
           │    calls DSP modules  │
           └───────────────────────┘
```

Inherits from `juce::AudioProcessor`. In `processBlock()`:
1. Reads all parameter values from AudioProcessorValueTreeState (freeze, stutter, speed, dry/wet, loop start/end, per-sample start/end, mixer levels).
2. Applies per-sample start/end fractions to the SamplerBank.
3. Processes MIDI — maps note numbers to sample indices (C1=pad 0, C2=pad 1, C3=pad 2, C4=pad 3) and triggers/stops samplers.
4. Generates sampler output via `samplerBank_.processBlock()`.
5. Mixes via `mixer_.processBlock()`.
6. Applies freeze via `freezeEffect_.processBlock()`.
7. In parallel mode, adds the raw input back into the output.

Also handles `loadSample(index, file)` using JUCE's `AudioFormatManager` and stores a waveform copy for the UI to display.

#### [`PluginEditor.h`](src/juce/PluginEditor.h) / [`.cpp`](src/juce/PluginEditor.cpp)

**Role in chain:** Top-level GUI window.

```
┌──────────────────────────────────────────────────┐
│  SAMPLER WITH FREEZE (title)                     │
├────────────────────────┬─────────────────────────┤
│  SampleTabPanel        │  Freeze controls        │
│  (tabs + waveforms)    │  (toggle, stutter,      │
│                        │   speed, dry/wet,       │
│                        │   loop start/end)       │
├────────────────────────┴─────────────────────────┤
│  Mixer controls (parallel mode, input/sampler)   │
└──────────────────────────────────────────────────┘
```

750×500 window divided into three sections. The left area is a `SampleTabPanel`, the right column holds freeze effect sliders/toggles, and the bottom strip has mixer controls. All controls are auto-synced to DSP parameters via JUCE `SliderAttachment` / `ButtonAttachment` / `ComboBoxAttachment`.

#### [`SampleTabPanel.h`](src/juce/SampleTabPanel.h) / [`.cpp`](src/juce/SampleTabPanel.cpp)

**Role in chain:** Per-sample editing UI (embedded in PluginEditor).

Four tabs labeled "Sample 1" through "Sample 4". Each tab contains:
- **MIDI note label** — shows the trigger note (C1, C2, C3, C4).
- **Sample name label** — filename of the loaded audio.
- **Load button** — opens an async file chooser (WAV/MP3/FLAC/AIF).
- **WaveformDisplay** — shows the audio waveform with draggable start/end markers.
- **Obey Note Off toggle** — per-sample setting that stops playback on MIDI note-off.

When the user drags a marker on the waveform, the panel pushes the new start/end fraction to the APVTS parameter (`sampleStart0`–`sampleStart3`, `sampleEnd0`–`sampleEnd3`), which flows to the DSP on the next audio block.

#### [`WaveformDisplay.h`](src/juce/WaveformDisplay.h) / [`.cpp`](src/juce/WaveformDisplay.cpp)

**Role in chain:** Visual feedback and region editing for one sample.

Custom JUCE `Component` that:
1. Downsamples the loaded audio to ~400 peaks for efficient drawing.
2. Draws the waveform as vertical bars — bright cyan inside the selected region, dim outside.
3. Renders a **green vertical line** (start marker, handle at top) and a **red vertical line** (end marker, handle at bottom).
4. Handles mouse drag to move markers, clamped so start < end with a minimum gap of 1%.
5. Fires `onRegionChanged(float start, float end)` callback on every drag update.

---

### Audio Utilities — [`src/audio/`](src/audio/)

#### [`AudioFileLoader.h`](src/audio/AudioFileLoader.h) / [`.cpp`](src/audio/AudioFileLoader.cpp)

**Role:** Reads audio files from disk into `float` vectors. Returns a `LoadResult` struct with audio data, sample rate, and channel count. Currently a skeleton — actual file loading in the JUCE build is handled by `PluginProcessor::loadSample()` using JUCE's `AudioFormatManager`.

#### [`PresetManager.h`](src/audio/PresetManager.h) / [`.cpp`](src/audio/PresetManager.cpp)

**Role:** Planned preset serialization system. Will save/load full plugin state (parameter values + embedded sample data) to JSON files. Currently a skeleton.

## Build Instructions

### Requirements

- C++17 compiler (MSVC, Clang++, or GCC)
- JUCE framework (cloned to `C:\JUCE`)
- CMake 3.24+

### Windows (MSVC)

```bash
cd vst_plugin_project
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### macOS (Xcode)

```bash
mkdir build && cd build
cmake .. -G Xcode
cmake --build . --config Release
```

### Linux (Ninja + Clang++)

```bash
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build .
```

### Teensy 4.1 (PlatformIO)

The Teensy firmware can be built using **PlatformIO** from within VS Code. The firmware shares
the `src/dsp/` core with the VST build — any DSP fix is immediately available to both targets.

```bash
# Prerequisites: VS Code with PlatformIO extension installed
# Open src/hardware/teensy/ as a PlatformIO project

# Build
platformio run -e teensy41

# Upload to device
platformio run -e teensy41 --target upload

# Serial monitor
platformio device monitor
```

Alternatively, open the project in VS Code with the **Wokwi** extension to simulate the Teensy
with virtual pots, buttons, and an SD card before touching hardware. See **Porting to Teensy 4.1**
below for the simulation strategy.

### Run Unit Tests

The Catch2 test suite validates the DSP core (`src/dsp/`) on your development machine.
These same tests remain valid after porting to Teensy — they form the numerical baseline
for the port and catch any regressions.

```bash
cd build
cmake --build . --config Release
./Release/DSPTests.exe          # Windows
./DSPTests                      # macOS / Linux
```

**When porting to Teensy:** Run this suite first (Phase B of the porting methodology)
to ensure all DSP logic is correct before any hardware glue is written.

## Development Roadmap

### VST Plugin Development

#### Phase 1: Core DSP — Completed ✓
- Project setup, CMakeLists.txt, DSP module structure
- All 6 DSP modules implemented (pure C++17, zero JUCE dependencies)
- Unit test suite with 100% core coverage

#### Phase 2: JUCE Integration — Completed ✓
- PluginProcessor (connects DSP to DAW audio/MIDI)
- Full GUI with freeze, stutter, speed, dry/wet, loop boundary controls
- Per-sample editor with waveform display and draggable markers
- MIDI triggering (C1, C2, C3, C4 → samples 1–4)
- Per-sample Obey Note Off toggle
- Build verified (VST3 + Standalone on Windows, macOS, Linux)

#### Phase 3: Polish & Testing — In Progress
- DAW testing (Reaper, Ableton, etc.)
- Preset system (save/load full state + embedded samples)
- File loading (WAV, MP3, FLAC via JUCE AudioFormatManager)

### Teensy 4.1 Hardware Port

#### Phase A: Compile Gate — TODO
- Set up `src/hardware/teensy/platformio.ini` with `src/dsp/` as shared source
- Create minimal `main.cpp` that instantiates DSP modules
- Verify PlatformIO build with zero compiler errors

#### Phase B: DSP Validation — TODO
- Run full Catch2 test suite to establish numeric baseline
- Any phase after this that breaks DSP output will be caught immediately

#### Phase C: Control Logic Simulation (Wokwi) — TODO
- Build virtual circuit in Wokwi (pots, buttons, LEDs)
- Implement `UIController` with real `analogRead`/`digitalRead`
- Validate control-to-DSP-call mapping via Serial output
- Confirm sample-select cycling and stutter zone mapping

#### Phase D: Sample Loading (Wokwi + SD) — TODO
- Implement WAV loader (`SampleLoader.cpp`)
- Add SD card simulation to Wokwi circuit
- Validate file parsing and PSRAM allocation

#### Phase E: Hardware Audio Integration — TODO
- Wire I2S codec (Teensy Audio Shield)
- Implement `I2SCodecDriver` using Teensy Audio library
- Wire USB MIDI (`usbMIDI` callbacks)
- Verify end-to-end audio on real hardware

See `docs/PORTING.md` for detailed methodology and Phase A–E breakdown.

## Usage (Once Complete)

1. Load plugin in compatible DAW (VST3)
2. Load audio files onto the 4 sample pads (via tabs in the UI)
3. Drag the green/red markers on each waveform to set the playback region
4. Trigger samples with MIDI notes (C1, C2, C3, C4)
5. Toggle per-sample "Obey Note Off" to control whether note-off stops playback
6. Hit freeze to capture the current audio loop
7. Twist stutter rate, speed, and loop boundaries
8. Toggle sequential/parallel to change what gets frozen
9. Save presets for later

## Porting to Teensy 4.1

The entire DSP core (`src/dsp/`) is **pure C++17 with zero JUCE dependencies**. It compiles on Teensy 4.1
without any modifications — the same code runs on both VST and hardware.

### Why This Works

- No virtual function calls, no dynamic allocation in real-time audio paths
- `std::atomic<>` for parameter changes (lock-free on ARM Cortex-M7)
- Fixed buffer sizes, known at compile time
- All timing and math is platform-agnostic

### Porting Strategy

The repository is structured as a **monorepo** where:

1. **`src/dsp/`** — Shared between VST (CMake) and Teensy (PlatformIO). Never changes between targets.
2. **`src/juce/`** — VST-specific JUCE wrapper. Discarded for Teensy.
3. **`src/hardware/teensy/`** — New Teensy hardware glue (I2S codec, MIDI, UI controls, SD card loader).
4. **`unit_tests/`** — Run on x86 to validate DSP logic. Pass/fail criteria for each porting phase.

### The Five Phases

The port is divided into five phases, each with a specific tool and pass/fail criterion:

| Phase | Goal | Tool | Pass Criterion |
|-------|------|------|----------------|
| **A** | Compile gate: Confirm `src/dsp/` is clean C++17 | PlatformIO build | Zero compiler errors |
| **B** | Numeric baseline: All DSP calculations correct | Catch2 unit tests | 0 test failures |
| **C** | Control logic: UI→DSP mapping works | Wokwi simulator | Serial output confirms all controls map correctly |
| **D** | File loading: WAV parser + PSRAM allocation works | Wokwi + SD sim | All 4 samples load, correct frame count |
| **E** | Hardware audio: I2S + USB MIDI + real DSP | Physical Teensy 4.1 | Audio plays, freeze works, stutter is rhythmic |

**Key insight:** Phases A–D use only the simulator + unit tests. Only Phase E needs physical hardware,
and by then the port is 80% validated without touching a device.

### Tools Used

- **PlatformIO** — Targets Teensy 4.1 with `platformio.ini` pointing at shared `src/dsp/`
- **Wokwi** — Simulates Teensy with virtual circuit (pots, buttons, SD card); runs compiled firmware
- **Catch2** — Validates DSP numeric correctness before any hardware integration
- **CMake** — Builds VST + unit tests on x86 (unchanged)

See `docs/PORTING.md` for detailed phase-by-phase breakdown, hardware layout, and WAV loader implementation.
See `docs/TEENSY_PERFORMANCE.md` for CPU budget calculations.

## License

(Add your license here)
