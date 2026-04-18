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
- **Loop start/end:** Shrink the loop region to create shorter stutter patterns
- **Read pointer jump:** The stutter mechanism — snaps the playhead back rhythmically

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
               │  Stutter: jumps read pointer on the beat     │
               │  Speed:   changes how fast the loop plays    │
               │  Dry/Wet: blend between original and frozen  │
               └──────────────────────────────────────────────┘ ──→ Output
```

**Stutter timing example at 120 BPM:**
```
  1/8 beat stutter = jumps every 0.25 seconds (fast choppy repeat)
  1/4 beat stutter = jumps every 0.5 seconds  (rhythmic repeat)
  1/2 beat stutter = jumps every 1.0 second   (slow echo feel)
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

- **4-Sample One-Shot Sampler** with tempo-synced looping
- **Echo Freeze Effect** on master output
- **Stutter Control** with rhythmic quantization (locked to BPM)
- **Speed Manipulation** for pitch shifting frozen audio
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
│   │   └── PluginEditor      — GUI (buttons, knobs)
│   ├── audio/            # File loading (WAV, MP3, FLAC)
│   │   ├── AudioFileLoader   — reads audio files into memory
│   │   └── PresetManager     — save/load presets with embedded samples
│   └── hardware/         # Future Teensy 4.1 hardware port
├── unit_tests/           # Automated tests (numeric validation, no audio)
├── docs/                 # Architecture and porting guides
└── CMakeLists.txt        # Build configuration
```

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

### Run Unit Tests

```bash
cd build
cmake --build . --config Release
./Release/DSPTests.exe          # Windows
./DSPTests                      # macOS / Linux
```

## Development Roadmap

### Phase 1: Completed ✓
- Project setup, CMakeLists.txt, DSP module structure
- All 6 DSP modules implemented
- Unit test suite (AudioBuffer, CircularBuffer, Sampler, SamplerBank, Mixer, FreezeEffect, Integration)

### Phase 2: In Progress
- JUCE integration (PluginProcessor, parameters, MIDI routing)
- Build and verify compilation

### Phase 3: TODO
- Test in DAW (Reaper)
- File loading (WAV, MP3, FLAC support)
- Preset system
- Freeze effect UI controls

### Phase 4: TODO
- Teensy 4.1 hardware adaptation
- Full testing and performance tuning

## Usage (Once Complete)

1. Load plugin in compatible DAW (VST3)
2. Load audio files onto the 4 sample pads
3. Trigger samples with MIDI notes (C3, C#3, D3, D#3)
4. Hit freeze to capture the current audio loop
5. Twist stutter rate, speed, and loop boundaries
6. Toggle sequential/parallel to change what gets frozen
7. Save presets for later

## Porting to Teensy

The DSP core (`src/dsp/`) has zero external dependencies — pure C++17. It can be compiled for Teensy 4.1 with no changes. See `docs/PORTING.md` for the hardware adaptation strategy and `docs/TEENSY_PERFORMANCE.md` for CPU budget calculations.

## License

(Add your license here)
