# Porting to Teensy

This document covers the full strategy for porting the sampler DSP core to Teensy 4.1
hardware — what stays the same, what needs to be rewritten, how the repository is
structured to keep both targets in sync, and the methodology for working through the
port incrementally using the toolchain available in this workspace.

---

## 1. Portability Rationale

All audio processing logic lives in `src/dsp/` with **zero JUCE dependencies**:

- `CircularBuffer`, `Sampler`, `SamplerBank`, `Mixer`, `FreezeEffect` are pure C++17
- No virtual function calls, no dynamic allocation in real-time paths
- Fixed buffer sizes (known at compile time or allocated once in `setup()`)
- `std::atomic<>` for parameter changes — lock-free on ARM Cortex-M7 (Teensy 4.1)

The JUCE layer (`src/juce/`) is a thin wrapper around this core. Replacing it for
Teensy means rewriting only the I/O glue, not the DSP.

---

## 2. What Stays vs. What Changes

### Shared DSP core — zero changes

```
src/dsp/
├── CircularBuffer.h/cpp       ✓ No changes
├── Sampler.h/cpp              ✓ No changes (per-pad gain, cached totalFrames_)
├── SamplerBank.h/cpp          ✓ No changes
├── Mixer.h/cpp                ✓ No changes
├── FreezeEffect.h/cpp         ✓ No changes (supports 1/32 + 1/64 stutter)
├── AudioBuffer.h/cpp          ✓ No changes
├── TeensyMenu.h/cpp           ✓ No changes (5-page menu, 3-way preset, pickup mode)
└── effects/
    ├── Effect.h               ✓ Abstract base
    ├── NoEffect.h             ✓ Passthrough
    ├── Distortion.h           ✓ fastTanh approximation (~28 cycles/sample)
    ├── BitCrush.h             ✓ Sample quantisation (~31 cycles/sample)
    ├── SimpleFilter.h         ✓ One-pole low-pass (~14 cycles/sample)
    └── EffectLibrary.h        ✓ Static factory (NUM_EFFECTS=4)
```

### JUCE glue — replaced entirely

The following exist only in `src/juce/` and have direct Teensy equivalents:

| JUCE concept | JUCE mechanism | Teensy replacement |
|---|---|---|
| Audio I/O | `AudioProcessor::processBlock()` | I2S codec ISR |
| MIDI input | `MidiBuffer` in processBlock | `usbMIDI` callbacks |
| Tempo | `PlayHead::getPosition()->getBpm()` | Pot or external clock |
| Parameter changes | `AudioProcessorValueTreeState` | `analogRead` / `digitalRead` in `loop()` |
| Sample loading | `juce::AudioFormatManager` + `AudioSampleBuffer` | Manual WAV parser + PSRAM |
| Per-sample gain | APVTS `sampleGain0-3` params | Pot + sample-select button |

### New Teensy-only files

Create these in `src/hardware/teensy/`:

```
src/hardware/teensy/
├── main.cpp / TeensySampler.cpp   # setup() + loop() entry point
├── I2SCodecDriver.h/cpp           # Audio codec interfacing (I2S)
├── MidiInput.h/cpp                # USB/Serial MIDI handler
├── UIController.h/cpp             # Pots, buttons, sample-select cycling
├── TempoController.h/cpp          # BPM from pot or external clock
├── SampleLoader.h/cpp             # WAV parser + PSRAM loader
└── PresetManager.h/cpp            # SD-backed preset save/restore (one preset per song)
```

---

## 3. Repository Layout & Toolchain

The Teensy firmware lives inside this repo as a PlatformIO project. The key design
principle is that **`src/dsp/` is a shared source root** — no file copying, no
duplication. Editing `FreezeEffect.cpp` once updates both the VST build and the next
Teensy firmware build automatically.

### Directory layout

```
vst_plugin_project/
├── src/
│   ├── dsp/                          ← shared by JUCE + Teensy (never changes)
│   ├── juce/                         ← VST-only wrapper
│   ├── audio/                        ← VST-only file loading
│   └── hardware/
│       └── teensy/
│           ├── platformio.ini        ← PlatformIO project config (Teensy 4.1 target)
│           ├── src/
│           │   ├── main.cpp          ← setup() / loop()
│           │   ├── UIController.cpp
│           │   ├── MidiInput.cpp
│           │   ├── SampleLoader.cpp
│           │   └── ...
│           └── wokwi.toml            ← Wokwi simulation config (VS Code extension)
├── unit_tests/                       ← Catch2 tests (run on x86, validate DSP logic)
├── CMakeLists.txt                    ← VST + unit test build (MSVC/Ninja/Xcode)
└── docs/
```

### PlatformIO configuration

`src/hardware/teensy/platformio.ini` points its source filter up to `src/dsp/`, so
the DSP core compiles directly into the firmware:

```ini
[env:teensy41]
platform  = teensy
board     = teensy41
framework = arduino
build_flags =
    -std=gnu++17
    -I../../dsp
build_src_filter =
    +<.>          ; src/hardware/teensy/src/
    +<../../dsp>  ; src/dsp/ — shared with VST build
```

Opening `src/hardware/teensy/` as a PlatformIO project in VS Code gives you
IntelliSense, build, upload, and serial monitor for the Teensy target alongside
the existing CMake VST project.

### Wokwi simulation

`wokwi.toml` ties the Wokwi simulator to the PlatformIO firmware binary. Once
configured, **Run Simulation** in VS Code launches the compiled firmware against a
virtual Teensy 4.1 with the wired circuit diagram:

```toml
[wokwi]
version = 1
firmware = ".pio/build/teensy41/firmware.hex"
elf      = ".pio/build/teensy41/firmware.elf"
```

The circuit diagram (`diagram.json`, also in `src/hardware/teensy/`) describes the
virtual hardware: potentiometers on analog pins, buttons on digital pins, LEDs.

### What each tool covers

| Concern | Tool | Coverage |
|---|---|---|
| DSP correctness (FreezeEffect, SamplerBank, …) | **Catch2** (`unit_tests/`) | Full numeric validation, runs on x86 |
| ARM compilation correctness | **PlatformIO** build | Catches Teensy-incompatible C++ at compile time |
| UI/control logic (button debouncing, pot mapping, stutter zones, sample-select cycling) | **Wokwi** simulator | No physical hardware needed |
| WAV loader (`loadWavFromSD`) | **Wokwi** + SD card simulation | Basic SD I/O |
| I2S audio output | Physical Teensy 4.1 only | No simulator substitute |
| USB MIDI | Physical Teensy 4.1 only | `usbMIDI` not simulated by Wokwi |

The combination of Catch2 + PlatformIO + Wokwi covers most of the port before
touching real hardware. Only I2S audio output and USB MIDI require the physical board.

---

## 4. Porting Methodology

The port is structured as five phases. Each phase has a clear tool and a pass/fail
criterion before moving on.

### Phase A — Compile gate (PlatformIO)

**Goal:** Confirm `src/dsp/` has no hidden JUCE or platform-specific dependencies.

1. Create `src/hardware/teensy/platformio.ini` with the config above.
2. Create a minimal `src/hardware/teensy/src/main.cpp` that instantiates
   `SamplerBank` and `FreezeEffect` and calls `prepare()` in `setup()` with no I/O.
3. Run **PlatformIO: Build** from VS Code.
4. **Pass criterion:** Zero compiler errors. Any error here is a real portability
   issue in `src/dsp/` that must be fixed before continuing.

### Phase B — DSP logic validation (Catch2)

**Goal:** Ensure all DSP behaviour is correct at the numeric level before any
hardware glue is written.

- Run the existing Catch2 suite (`unit_tests/`). All tests must pass.
- Add any missing tests for edge cases discovered during Phase A.
- **Pass criterion:** 0 test failures. This baseline is the reference point for
  the rest of the port — if a later phase breaks DSP output, this suite will catch it.

### Phase C — Control logic simulation (Wokwi)

**Goal:** Validate the entire UI control layer (pots, buttons, sample-select cycling,
stutter zone mapping) without needing hardware.

1. Build the `diagram.json` circuit in Wokwi (or use the VS Code Wokwi extension's
   visual editor): 10 pots on analog pins, 6 buttons on digital pins, 4 LEDs.
2. Implement `UIController` with real `analogRead` / `digitalRead` calls.
3. Add `Serial.print()` traces for each control change.
4. **Run Simulation** in VS Code — twist virtual pots, press virtual buttons.
5. **Pass criterion:** Serial output shows correct DSP calls for every control
   gesture. The sample-select button cycles pads 0→1→2→3→0. Stutter pot maps to
   exactly 7 zones. All gain/loop-pos/loop-len fractions are in range.

### Phase D — Sample loading (Wokwi + SD simulation)

**Goal:** Validate `loadWavFromSD()` without a physical SD card.

1. Implement `SampleLoader.cpp` (see the WAV loader code in section 8).
2. In Wokwi, add an SD card component to `diagram.json` and place
   `sample0.wav`–`sample3.wav` (16-bit mono 48kHz WAV, prepared with ffmpeg)
   in the Wokwi project's virtual SD root.
3. Call `loadWavFromSD()` in `setup()` and trace the result over Serial.
4. **Pass criterion:** Serial confirms all 4 samples loaded, correct frame count,
   no error return. Trigger buttons play audible output once I2S is connected.

### Phase E — Audio integration (physical hardware)

**Goal:** End-to-end audio on the real Teensy 4.1.

1. Wire the I2S codec (e.g., SGTL5000 on the Teensy Audio Shield).
2. Implement `I2SCodecDriver` using the Teensy Audio library's `AudioStream`
   interface or raw DMA.
3. Wire USB MIDI (`usbMIDI` callbacks → `SamplerBank::triggerSample`).
4. Upload via PlatformIO, connect headphones.
5. **Pass criterion:** Triggering pads produces audio. Freeze engages. Stutter is
   rhythmically correct at 120 BPM. No audio glitches under full 4-pad load.

---

## 5. JUCE → Teensy Translation Reference

### 5.1 Audio I/O

**JUCE:** `AudioProcessor::processBlock()` called by host
```cpp
void PluginProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    // Audio data already in buffer from host
}
```

**Teensy:** Raw I2S codec ISR (or Teensy Audio library `AudioStream::update()`)
```cpp
void handleAudioInterrupt() {
    float inputBuffer[BLOCK_SIZE];
    readFromCodec(inputBuffer);

    float outputBuffer[BLOCK_SIZE];
    processAudio(inputBuffer, outputBuffer, BLOCK_SIZE);

    writeToCodec(outputBuffer);
}
```

### 5.2 MIDI Input

**JUCE:** Host sends MIDI via `MidiBuffer` in processBlock()
```cpp
for (auto metadata : midiMessages) {
    handleMidiMessage(metadata.getMessage());
}
```

**Teensy:** USB MIDI or Serial MIDI
```cpp
void setup() {
    usbMIDI.setHandleNoteOn(handleMidiNoteOn);
}

void handleMidiNoteOn(byte channel, byte note, byte velocity) {
    samplerBank_.triggerSample(note % 4, 4.0, globalTempo);
}
```

### 5.3 Tempo Sync

**JUCE:** Host provides tempo via `PlayHead::getPosition()`
```cpp
double getTempo() const {
    if (auto pos = getPlayHead()->getPosition())
        if (auto bpm = pos->getBpm())
            return bpm.value();
    return 120.0;
}
```

**Teensy:** Local tempo pot or external clock input
```cpp
double globalTempo = 120.0;

void updateTempoKnob() {
    int analogValue = analogRead(TEMPO_PIN);
    globalTempo = map(analogValue, 0, 1023, 40, 200);  // 40–200 BPM
}
```

### 5.4 Parameter Changes

**JUCE:** Host sends parameter updates via `AudioProcessorValueTreeState`
```cpp
dsp::freezeEffect_.setLoopLength(parameterValue);
```

**Teensy:** Local UI controls read in `loop()`
```cpp
void updateUI() {
    freezeEffect_.setLoopLength(readPotentiometer(LOOP_LENGTH_PIN) / 1023.0f);
    freezeEffect_.setLoopPosition(readPotentiometer(LOOP_POS_PIN) / 1023.0f);
    freezeEffect_.setFrozen(readButton(FREEZE_BUTTON));
}
```

### 5.5 Per-Sample Gain

The DSP layer supports per-pad volume via `SamplerBank::setSampleGain(index, gain)`.
In JUCE this is driven by the APVTS `sampleGain0-3` parameters; on Teensy it is
driven by a single pot whose target pad is selected by the sample-select button.

```cpp
void updatePerSampleControls() {
    // currentSampleTab is toggled by the sample-select button (0-3)
    float gainValue = analogRead(SAMPLE_GAIN_POT) / 1023.0f * 2.0f;  // 0.0–2.0
    samplerBank_.setSampleGain(currentSampleTab, gainValue);
}
```

## 6. Planned Teensy Hardware Layout

The Teensy version uses physical controls instead of a GUI window.
All DSP calls are identical; only the control source changes.

### Buttons (digital pins)
| Button | Function |
|--------|----------|
| Trigger 1–4 | Trigger sample pads 0–3 (replaces MIDI note-on) |
| Freeze | Toggle freeze on/off |
| Series/Parallel | Toggle mixer routing mode |
| Sample Select | Cycle through pads 0–3 for per-sample controls |

### Potentiometers (analog pins)
| Pot | Function | DSP call |
|-----|----------|----------|
| Stutter Rate | Select stutter division (1 beat → 1/64 beat) | `setStutterFraction()` |
| Speed | Playback speed 0.25×–4× | `setPlaybackSpeed()` |
| Loop Length | Fraction of buffer used as loop window | `setLoopLength()` |
| Loop Position | Shift the loop window within the buffer | `setLoopPosition()` |
| Input Level | Mixer input gain | `setInputLevel()` |
| Sampler Level | Mixer sampler gain | `setSamplerLevel()` |
| **Sample Gain** | Per-pad volume (acts on selected pad) | `setSampleGain()` |
| **Sample Loop Pos** | Per-pad loop position fraction (acts on selected pad) | `setSampleLoopPosFraction()` |
| **Sample Loop Len** | Per-pad loop length fraction (acts on selected pad) | `setSampleLoopLenFraction()` |

The **Sample Select** button selects which pad the per-sample pots (gain, loop pos, loop len)
currently control. An LED or small display indicates the active pad (1–4).

### Stutter Rate Mapping
The stutter pot maps to 7 discrete values (pot range divided into 7 zones):
```cpp
static constexpr double kStutterValues[7] = {
    1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.015625
};  // 1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64 beat

int zone = analogRead(STUTTER_POT) * 7 / 1024;
freezeEffect_.setStutterFraction(kStutterValues[zone]);
```

## 7. Minimal Teensy Example

```cpp
// src/hardware/teensy/TeensySampler.cpp

#include "CircularBuffer.h"
#include "SamplerBank.h"
#include "FreezeEffect.h"

SamplerBank samplerBank;
FreezeEffect freezeEffect;
double globalTempo = 120.0;

void setup() {
    constexpr int SAMPLE_RATE = 48000;
    constexpr int BLOCK_SIZE = 256;
    
    samplerBank.prepare(SAMPLE_RATE, BLOCK_SIZE);
    freezeEffect.prepare(SAMPLE_RATE, BLOCK_SIZE);
    
    usbMIDI.setHandleNoteOn(handleMidiNoteOn);
    setupI2SAudio();
}

int currentSampleTab = 0;  // Which pad the per-sample knobs control

void handleMidiNoteOn(byte channel, byte note, byte velocity) {
    int sampleIndex = (note - 60) % 4;  // C3 = sample 0
    samplerBank.triggerSample(sampleIndex, 4.0, globalTempo);
}

void loop() {
    // Handle USB MIDI
    while (usbMIDI.read()) {}
    
    // Update global controls (freeze, mixer, tempo)
    updateGlobalUI();
    
    // Update per-sample controls (gain, start, end) for the selected pad
    updatePerSampleControls();
}

void updateGlobalUI() {
    freezeEffect.setFrozen(readButton(FREEZE_BUTTON));
    freezeEffect.setPlaybackSpeed(mapFloat(readPot(SPEED_POT), 0, 1023, 0.25f, 4.0f));
    
    int stutterZone = readPot(STUTTER_POT) * 7 / 1024;
    freezeEffect.setStutterFraction(kStutterValues[stutterZone]);
    
    freezeEffect.setLoopLength(readPot(LOOP_LENGTH_POT) / 1023.0f);
    freezeEffect.setLoopPosition(readPot(LOOP_POS_POT) / 1023.0f);
    
    bool parallel = readButton(PARALLEL_BUTTON);
    mixer.setMode(parallel ? Mixer::Mode::Parallel : Mixer::Mode::Sequential);
    mixer.setInputLevel(readPot(INPUT_LEVEL_POT) / 1023.0f);
    mixer.setSamplerLevel(readPot(SAMPLER_LEVEL_POT) / 1023.0f);
}

void updatePerSampleControls() {
    // Sample-select button cycles 0->1->2->3->0...
    if (readButtonRising(SAMPLE_SELECT_BUTTON))
        currentSampleTab = (currentSampleTab + 1) % 4;
    
    // Per-pad volume (0.0 - 2.0 range)
    float gain = readPot(SAMPLE_GAIN_POT) / 1023.0f * 2.0f;
    samplerBank.setSampleGain(currentSampleTab, gain);
    
    // Per-pad loop position/length region
    float loopPos = readPot(SAMPLE_LOOP_POS_POT) / 1023.0f;
    float loopLen = readPot(SAMPLE_LOOP_LEN_POT) / 1023.0f;
    samplerBank.setSampleLoopPosFraction(currentSampleTab, loopPos);
    samplerBank.setSampleLoopLenFraction(currentSampleTab, loopLen);
}
```

## 9. Memory Considerations

**Teensy Memory Limits:**
- Teensy 4.1: 8MB PSRAM (can be used for sample storage)
- On-chip RAM: 512KB (too small for large freeze buffers)

**Recommendations:**
- Store samples in external PSRAM if available
- Default freeze buffer: ~2 seconds at 48kHz = 96KB (fits in on-chip)
- Max 4 samples @ 5 seconds each = 960KB total (fits in PSRAM)

**Memory-Safe Pattern for Teensy:**
```cpp
// Allocate buffers once in setup(), never in loop/audio callback
void setup() {
    expandPSRAM();  // Enable PSRAM if available
    freezeEffect.setBufferSizeInSamples(48000 * 4);  // 4 seconds @ 48kHz
}

// No allocations in audio callback!
void audioCallback() {
    // Only read/write pre-allocated buffers
    freezeEffect.processBlock(input, output, BLOCK_SIZE, globalTempo);
}
```

## 8. Sample Loading: The One Real Divergence

This is the area where the Teensy port **cannot share code** with the VST version.
Everything else in `src/juce/` can simply be discarded and the `src/dsp/` core reused
as-is. Sample loading is different because the two environments have fundamentally
different ways of getting audio data into memory.

### What the VST version does

`src/juce/PluginProcessor.cpp → loadSample()` relies on two JUCE classes:

- **`juce::AudioFormatManager`** — knows how to decode WAV, MP3, AIFF, FLAC, etc.
  by selecting the correct `AudioFormatReader` from the file extension.
- **`juce::AudioSampleBuffer`** — a heap-allocated float buffer that `AudioFormatReader`
  decodes into. Channel 0 is then passed to `SamplerBank::loadSampleData()`.

Neither class exists on Teensy. `AudioFormatManager` depends on JUCE's file I/O layer
and codec libraries. `AudioSampleBuffer` is a JUCE-specific container.

### What needs to be written for Teensy

A custom `loadSampleFromSD()` function that:

1. Opens a file on the SD card using the Teensy SD library (`SD.h`).
2. Parses the file format manually (or uses a lightweight 3rd-party decoder).
3. Allocates a `std::vector<float>` or pre-sized array in PSRAM.
4. Calls `samplerBank_.loadSampleData(index, buffer, numSamples)` — **this call is identical to the VST version**.

**WAV is the practical choice for Teensy.** WAV files are PCM data with a small
header; parsing them is ~30 lines of C++ with no external library. MP3/FLAC decoding
would require libraries that are complex to port and expensive in flash space.

```cpp
// src/hardware/teensy/SampleLoader.cpp  (new file, Teensy only)
#include <SD.h>
#include "SamplerBank.h"

// Minimal WAV header layout (44-byte standard PCM header).
struct WavHeader {
    char     riff[4];         // "RIFF"
    uint32_t fileSize;
    char     wave[4];         // "WAVE"
    char     fmt[4];          // "fmt "
    uint32_t fmtSize;         // 16 for PCM
    uint16_t audioFormat;     // 1 = PCM
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;   // 16 for standard WAV
    char     data[4];         // "data"
    uint32_t dataSize;
};

bool loadWavFromSD(SamplerBank& bank, int padIndex, const char* filename)
{
    File f = SD.open(filename);
    if (!f)
        return false;

    WavHeader header;
    f.read(reinterpret_cast<uint8_t*>(&header), sizeof(WavHeader));

    // Basic validation: PCM, 16-bit, mono or stereo only.
    if (header.audioFormat != 1 || header.bitsPerSample != 16)
    {
        f.close();
        return false;   // Only standard 16-bit PCM supported.
    }

    int numFrames = header.dataSize / (header.numChannels * 2);

    // Allocate in PSRAM (external 8 MB on Teensy 4.1).
    // extmem_malloc allocates from PSRAM; never use in the audio callback!
    float* pcm = (float*)extmem_malloc(numFrames * sizeof(float));
    if (!pcm)
    {
        f.close();
        return false;
    }

    // Read channel 0 only, converting 16-bit integers to normalised floats.
    // This mirrors what PluginProcessor::loadSample() does (takes channel 0 of
    // the JUCE AudioSampleBuffer and passes it to SamplerBank::loadSampleData).
    for (int i = 0; i < numFrames; ++i)
    {
        int16_t sample;
        f.read(reinterpret_cast<uint8_t*>(&sample), 2);
        pcm[i] = sample / 32768.0f;          // Normalise to [-1, +1]
        if (header.numChannels > 1)
            f.seek(f.position() + 2);         // Skip remaining channels
    }
    f.close();

    // This call is identical to the VST version.
    bank.loadSampleData(padIndex, pcm, numFrames);

    extmem_free(pcm);   // SamplerBank copies the data internally via setSampleData().
    return true;
}
```

### Key differences vs. VST version

| Aspect | VST (`PluginProcessor::loadSample`) | Teensy (`loadWavFromSD`) |
|--------|--------------------------------------|--------------------------|
| File source | OS filesystem via JUCE `File` | SD card via `SD.h` |
| Format support | WAV, MP3, AIFF, FLAC (JUCE decodes) | WAV only (parsed manually) |
| Decoding | JUCE `AudioFormatReader` | Manual 44-byte header parse |
| Buffer type | `juce::AudioSampleBuffer` | Raw float array in PSRAM |
| DSP handoff | `samplerBank_.loadSampleData(i, ch0, n)` | **identical call** |
| Waveform copy | Stored in `sampleWaveforms_[]` for UI | Not needed (no waveform display) |

The last row is the key insight: the Teensy version does not need to store a
waveform copy because there is no `WaveformDisplay` component. This saves
significant PSRAM and simplifies the loader.

### Preparing samples for Teensy

Convert any source audio to 16-bit mono WAV at 48 kHz before copying to SD card:

```bash
# Using ffmpeg (cross-platform)
ffmpeg -i input.mp3 -ar 48000 -ac 1 -c:a pcm_s16le output.wav
```

Files should be named `sample0.wav`, `sample1.wav`, `sample2.wav`, `sample3.wav`
and placed in the root of the SD card so `loadWavFromSD` can find them in `setup()`.

## 10. Porting Checklist

### Phase A — Compile gate
- [ ] Create `src/hardware/teensy/platformio.ini`
- [ ] Create minimal `main.cpp` (no I/O, just `prepare()` calls)
- [ ] PlatformIO build passes with zero errors

### Phase B — DSP logic validation
- [ ] All Catch2 unit tests pass (`unit_tests/`)
- [ ] Add tests for any edge cases found in Phase A

### Phase C — Control logic simulation (Wokwi)
- [ ] Build `diagram.json` circuit (10 pots, 6 buttons, 4 LEDs)
- [ ] Implement `UIController` with real `analogRead`/`digitalRead`
- [ ] Serial output confirms correct DSP calls for every control
- [ ] Sample-select cycling verified (0→1→2→3→0)
- [ ] Stutter zone mapping verified (7 discrete zones)
- [ ] All gain/loop-pos/loop-len fractions confirmed in range

### Phase D — Sample loading (Wokwi + SD)
- [ ] Implement `SampleLoader.cpp` (WAV parser)
- [ ] Add SD card component to `diagram.json`
- [ ] Prepare test WAVs with ffmpeg (16-bit mono 48kHz)
- [ ] Serial confirms all 4 samples loaded with correct frame count
- [ ] `loadWavFromSD` returns no errors

### Phase E — Audio integration (physical hardware)
- [ ] Wire I2S codec (Teensy Audio Shield or equivalent)
- [ ] Implement `I2SCodecDriver`
- [ ] Wire USB MIDI (`usbMIDI` callbacks → `SamplerBank::triggerSample`)
- [ ] Upload via PlatformIO
- [ ] Trigger pads produce audio
- [ ] Freeze engages and loops correctly
- [ ] Stutter is rhythmically correct at 120 BPM
- [ ] No audio glitches under full 4-pad load

### Phase F — Polish
- [ ] Wire mixer controls (parallel toggle, input/sampler level)
- [ ] Wire all freeze pots (stutter, speed, dry/wet, loop pos/len)
- [ ] Wire per-sample pots (gain, loop pos, loop len) + sample-select cycling
- [ ] Performance profiling and optimization

### Phase G — Preset system (see section 11)
- [ ] Define `Preset` struct (sample paths + all parameter values)
- [ ] Implement `PresetManager::loadPreset(n)` (reads `.PRE` → DSP + WAVs)
- [ ] Implement `PresetManager::savePreset(n)` (writes live state → `.PRE`)
- [ ] Wire preset-select button/encoder to `loadPreset()`
- [ ] Wire save gesture (hold SAVE button ≥ 2 s) to `savePreset()`
- [ ] Confirm that loading a preset reverts unsaved parameter changes
- [ ] Confirm that twisting a pot after load sets `dirty_` without touching SD
- [ ] Test missing-file path (WAV deleted from SD) → graceful skip
- [ ] Test ≥ 10 presets round-trip on real SD card without corruption

---

## 11. Preset Management System

### Motivation

The Teensy version is intended to be a live performance instrument where each song
has its own set of samples and parameter values. When you load a preset, the machine
should be in exactly the state you saved it in. If you then tweak a knob, the change
should apply live but not be persisted until you explicitly save. Reloading the preset
discards any unsaved edits — identical semantics to a hardware sampler's "compare"
button.

This is the hardware equivalent of the VST's `getStateInformation` / `setStateInformation`
(implemented in `src/juce/PluginProcessor.cpp`), but reading and writing to an SD card
instead of a DAW `MemoryBlock`.

---

### SD Card Layout

Two layouts are practical. Choose based on whether samples are shared across songs:

**Option A — shared sample pool (samples reused across presets):**
```
SD:/
├── SAMPLES/
│   ├── kick.wav
│   ├── snare.wav
│   └── ...              ← samples referenced by path in each preset
└── PRESETS/
    ├── SONG_01.PRE
    ├── SONG_02.PRE
    └── ...
```

**Option B — self-contained preset folders (recommended for live use):**
```
SD:/
├── SONG_01/
│   ├── preset.cfg       ← parameter values (text or binary)
│   ├── sample0.wav
│   ├── sample1.wav
│   ├── sample2.wav
│   └── sample3.wav
├── SONG_02/
│   └── ...
```
Option B is safer for live use: renaming or moving the folder keeps the preset
self-contained. The path stored in the config can be relative (`sample0.wav`
means `<preset_folder>/sample0.wav`).

---

### Preset Struct

```cpp
// src/hardware/teensy/PresetManager.h

struct Preset {
    // Sample paths (relative to the preset folder on SD)
    char samplePaths[4][64];

    // FreezeEffect parameters
    float loopLength;       // 0.0–1.0
    float loopPosition;     // 0.0–1.0
    float stutterFraction;  // one of the 7 kStutterValues
    float playbackSpeed;    // 0.25–4.0
    bool  freezeEnabled;

    // Per-pad parameters
    float sampleGain[4];        // 0.0–2.0
    float sampleStart[4];       // 0.0–1.0
    float sampleEnd[4];         // 0.0–1.0
    bool  obeyNoteOff[4];

    // Mixer parameters
    float inputLevel;
    float samplerLevel;
    bool  parallelMode;
};
```

The struct can be written as plain binary (`fwrite`/`fread`, ~200 bytes) or as a
human-readable key=value text file. Text is preferable for debugging and manual
editing; binary is simpler to implement.

---

### PresetManager Class

```cpp
// src/hardware/teensy/PresetManager.h

class PresetManager {
public:
    // Reads preset N from SD, loads WAVs into PSRAM, applies all parameters
    // to the live DSP objects. Clears dirty_.
    // loadPreset() mutes audio output while WAVs are being decoded from SD.
    void loadPreset(int n);

    // Writes the current liveState_ back to preset N on SD. Clears dirty_.
    // Call only from a button gesture (hold ≥ 2 s), never from the audio ISR.
    void savePreset(int n);

    // Returns true if any parameter has been changed since the last load/save.
    bool isDirty() const { return dirty_; }

    // --- Setters called by UIController for each control ---
    // Each setter: updates liveState_, applies to DSP object, sets dirty_ = true.
    void setLoopLength(float v);
    void setLoopPosition(float v);
    void setStutterFraction(float v);
    void setPlaybackSpeed(float v);
    void setFreezeEnabled(bool v);
    void setSampleGain(int pad, float v);
    void setSampleStart(int pad, float v);
    void setSampleEnd(int pad, float v);
    void setObeyNoteOff(int pad, bool v);
    void setInputLevel(float v);
    void setSamplerLevel(float v);
    void setParallelMode(bool v);

private:
    Preset      liveState_;
    bool        dirty_ = false;
    int         currentPreset_ = -1;

    // References to the live DSP objects (injected via constructor).
    SamplerBank& samplerBank_;
    FreezeEffect& freezeEffect_;
    Mixer&        mixer_;

    void applyPresetToDsp(const Preset& p);
    bool loadPresetFile(int n, Preset& out);
    bool savePresetFile(int n, const Preset& p);
};
```

---

### Parameter Change Flow

```
loop()
  └─ UIController reads pot/button
       └─ PresetManager::setLoopLength(v)
            ├─ liveState_.loopLength = v        (update saved state mirror)
            ├─ dirty_ = true                    (flag unsaved changes)
            └─ freezeEffect_.setLoopLength(v)   (apply to live DSP immediately)

Audio ISR (I2S)
  └─ freezeEffect_.processBlock(...)            (reads parameters atomically)
       └─ no SD, no preset, no state knowledge
```

The audio ISR is completely unaware of the preset system. It only reads the DSP
object state, which is kept up to date by the setters above. The `std::atomic<>`
parameters already in `FreezeEffect` make this lock-free on the Cortex-M7.

**Loading a preset:**
```
Button press → PresetManager::loadPreset(n)
  ├─ mute audio output (write silence flag read by ISR)
  ├─ read SONG_0N/preset.cfg from SD
  ├─ for each pad: loadWavFromSD(bank, i, path)  ← may take ~100–500 ms
  ├─ applyPresetToDsp(liveState_)               ← all DSP atomics updated
  ├─ unmute audio
  └─ dirty_ = false
```

**Saving a preset (hold SAVE ≥ 2 s):**
```
Button hold → PresetManager::savePreset(n)
  ├─ write liveState_ → SONG_0N/preset.cfg on SD
  └─ dirty_ = false
```

If you press load without saving, `dirty_` is discarded and the last-saved values
are restored — the "compare" / "revert" behaviour.

---

### Key Constraints

| Concern | Solution |
|---|---|
| WAV loading takes 100–500 ms | Mute audio (write silence) during `loadPreset()`; unmute when done |
| SD writes during audio = glitches | Only call `savePreset()` from a timed button hold gesture, never from ISR |
| PSRAM for 4 samples | 4 × 5 s × 48kHz × 4 bytes ≈ 3.8 MB. Fits in 8 MB PSRAM |
| Missing WAV on SD | `loadWavFromSD` returns `false`; log to Serial, leave that pad silent |
| Flash/EEPROM wear | Store presets on SD only — never EEPROM |
| Preset count | SD can hold as many presets as space allows; no firmware change needed |

---

### Relationship to VST State Persistence

The Teensy `PresetManager` is structurally identical to the VST's
`getStateInformation` / `setStateInformation` implemented in
`src/juce/PluginProcessor.cpp`:

| Aspect | VST | Teensy |
|---|---|---|
| State container | APVTS XML in `MemoryBlock` | `Preset` struct in `.PRE` file on SD |
| Sample paths | `samplePaths_[4]` stored in XML | `Preset::samplePaths[4][64]` |
| Restore trigger | DAW project load | Preset-select button |
| Save trigger | DAW project save (automatic) | Hold SAVE button ≥ 2 s |
| Dirty tracking | DAW handles it | `PresetManager::dirty_` flag |
| File I/O | `copyXmlToBinary` / `getXmlFromBinary` | `fwrite` / `fread` on SD |

---

## 12. Debugging Tips

- Use `Serial.print()` for logging — but not inside the audio ISR (may cause glitches)
- Monitor CPU load with Teensy Audio library's `AudioProcessorUsage()`
- Use Wokwi serial monitor to trace control changes before touching hardware
- In Phase A/B, any compile errors in `src/dsp/` under PlatformIO must be fixed
  in `src/dsp/` itself (not worked around), so the VST build stays in sync

See `src/hardware/teensy/` for implementation files as they are created.

---

## 13. TeensyMenu — Portability of the UI Emulation Layer

### Overview

The `TeensyMenu` DSP class (`src/dsp/TeensyMenu.h/cpp`) is **fully portable** — pure
C++17, no JUCE, no display-specific code. It produces text strings for 4 zones and
exposes `getZoneText(int zone)` / `getRow(int row)`. Writing those strings to any
2×16 character LCD is straightforward.

The JUCE wrapper `TeensyEmulationPanel` (`src/juce/TeensyEmulationPanel.h/cpp`) is
VST-only and has no Teensy equivalent — it is replaced by direct LCD calls in
`UIController`.

### Implementing the LCD output on Teensy

```cpp
// UIController.cpp (Teensy)
#include <LiquidCrystal_I2C.h>   // or LiquidCrystal for parallel LCD
#include "TeensyMenu.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);
TeensyMenu menu;

void updateLCDDisplay() {
    lcd.setCursor(0, 0);
    lcd.print(menu.getRow(0).c_str());
    lcd.setCursor(0, 1);
    lcd.print(menu.getRow(1).c_str());
}
```

---

### ⚠ VST-only feature: negative-character (inverted) highlighting

In the VST's `TeensyEmulationPanel`, the selected function on the Preset page
(Save or Reload) is highlighted using **inverted colours** — the zone gets a solid
bright-green fill with black text, mimicking a pixel-level "negative character"
effect. This is drawn in JUCE using `Graphics::fillRect()` before `drawText()`.

**Standard 16×2 character LCDs (HD44780-compatible) do not support this.**
The HD44780 controller renders characters from a fixed ROM; there is no way to
invert individual character cells without replacing them with custom characters
(of which only 8 are available and they are 5×8 pixels — too coarse for legible
inverted text).

**Adaptation for Teensy: use a `>` cursor prefix instead.**

Replace the inverted-colour highlight with a `>` marker prepended to the selected
zone's text. `TeensyMenu::getZoneText()` already handles this: on the Preset page,
the selected function's zone text must be formatted to lead with `>`.

Update `TeensyMenu::getZoneText()` for zone 2 and 3 on the Preset page:

```cpp
case 2:  // Zone 3 — "save"
    if (currentPage_ == Page::Preset) {
        bool selected = (selectedFunction_ == PresetFunction::Save);
        return fitToWidth(selected ? ">save" : " save", 8);
    }
    // ... effect name on sample pages (unchanged)

case 3:  // Zone 4 — "reload"
    if (currentPage_ == Page::Preset) {
        bool selected = (selectedFunction_ == PresetFunction::Reload);
        return fitToWidth(selected ? ">reload" : " reload", 8);
    }
    // ... effect value on sample pages (unchanged)
```

This gives ">`save`   " / " `reload` " on the LCD when Save is selected, and
" `save`   " / ">`reload`" when Reload is selected — identical information density
to the inverted highlighting, and fully compatible with any character LCD.

> **Note for VST build:** When the `>` cursor approach is adopted, the inverted
> background drawing in `TeensyEmulationPanel::paint()` can be kept for visual
> flair, or removed for exact LCD parity — your choice.
