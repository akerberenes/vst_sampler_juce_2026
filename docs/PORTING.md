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
├── Sampler.h/cpp              ✓ No changes (includes per-pad gain)
├── SamplerBank.h/cpp          ✓ No changes (setSampleGain forwarding)
├── Mixer.h/cpp                ✓ No changes
├── FreezeEffect.h/cpp         ✓ No changes (supports 1/32 + 1/64 stutter)
└── AudioBuffer.h/cpp          ✓ No changes
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
└── SampleLoader.h/cpp             # WAV parser + PSRAM loader
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
   exactly 7 zones. All gain/start/end fractions are in range.

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
dsp::freezeEffect_.setDryWetMix(parameterValue);
```

**Teensy:** Local UI controls read in `loop()`
```cpp
void updateUI() {
    freezeEffect_.setDryWetMix(readPotentiometer(DRY_WET_PIN) / 1023.0f);
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
| Dry/Wet | Freeze mix 0–100% | `setDryWetMix()` |
| Loop Start | Freeze loop start position | `setLoopStart()` |
| Loop End | Freeze loop end position | `setLoopEnd()` |
| Input Level | Mixer input gain | `setInputLevel()` |
| Sampler Level | Mixer sampler gain | `setSamplerLevel()` |
| **Sample Gain** | Per-pad volume (acts on selected pad) | `setSampleGain()` |
| **Sample Start** | Per-pad start fraction (acts on selected pad) | `setSampleStartFraction()` |
| **Sample End** | Per-pad end fraction (acts on selected pad) | `setSampleEndFraction()` |

The **Sample Select** button selects which pad the per-sample pots (gain, start, end)
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
    freezeEffect.setDryWetMix(readPot(DRY_WET_POT) / 1023.0f);
    freezeEffect.setPlaybackSpeed(mapFloat(readPot(SPEED_POT), 0, 1023, 0.25f, 4.0f));
    
    int stutterZone = readPot(STUTTER_POT) * 7 / 1024;
    freezeEffect.setStutterFraction(kStutterValues[stutterZone]);
    
    freezeEffect.setLoopStart(readPot(LOOP_START_POT) / 1023.0f);
    freezeEffect.setLoopEnd(readPot(LOOP_END_POT) / 1023.0f);
    
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
    
    // Per-pad start/end region
    float start = readPot(SAMPLE_START_POT) / 1023.0f;
    float end   = readPot(SAMPLE_END_POT)   / 1023.0f;
    samplerBank.setSampleStartFraction(currentSampleTab, start);
    samplerBank.setSampleEndFraction(currentSampleTab, end);
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
- [ ] All gain/start/end fractions confirmed in range

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
- [ ] Wire all freeze pots (stutter, speed, dry/wet, loop start/end)
- [ ] Wire per-sample pots (gain, start, end) + sample-select cycling
- [ ] Add preset storage (SD card)
- [ ] Performance profiling and optimization

---

## 11. Debugging Tips

- Use `Serial.print()` for logging — but not inside the audio ISR (may cause glitches)
- Monitor CPU load with Teensy Audio library's `AudioProcessorUsage()`
- Use Wokwi serial monitor to trace control changes before touching hardware
- In Phase A/B, any compile errors in `src/dsp/` under PlatformIO must be fixed
  in `src/dsp/` itself (not worked around), so the VST build stays in sync

See `src/hardware/teensy/` for implementation files as they are created.
