# Porting to Teensy

This document outlines how to port the sampler DSP core to Teensy hardware.

## Why This Codebase is Portable

All audio processing logic lives in `src/dsp/` with **zero JUCE dependencies**. This means:

- CircularBuffer, Sampler, SamplerBank, Mixer, FreezeEffect are pure C++
- No virtual function calls, dynamic allocation in real-time paths
- Fixed buffer sizes (known at compile time or allocated once)
- Thread-safe atomic operations for parameter changes

## JUCE Dependencies to Replace

The following JUCE-specific functionality **exists only in `src/juce/`** and **must be reimplemented for Teensy**:

### 1. **Audio I/O**

**JUCE:** `AudioProcessor::processBlock()` called by host
```cpp
// JUCE version
void PluginProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    // Audio data already in buffer from host
}
```

**Teensy:** Raw I2S codec driver
```cpp
// Teensy version (pseudocode)
void handleAudioInterrupt() {
    float inputBuffer[BLOCK_SIZE];
    readFromCodec(inputBuffer);
    
    // Call pure DSP processBlock equivalent
    float outputBuffer[BLOCK_SIZE];
    processAudio(inputBuffer, outputBuffer, BLOCK_SIZE);
    
    writeToCodec(outputBuffer);
}
```

### 2. **MIDI Input**

**JUCE:** Host sends MIDI via `MidiBuffer` in processBlock()
```cpp
// JUCE version
for (auto metadata : midiMessages) {
    handleMidiMessage(metadata.getMessage());
}
```

**Teensy:** USB MIDI or Serial MIDI
```cpp
// Teensy version
void setup() {
    usbMIDI.setHandleNoteOn(handleMidiNoteOn);
}

void handleMidiNoteOn(byte channel, byte note, byte velocity) {
    samplerBank_.triggerSample(note % 4, 4.0, globalTempo);
}
```

### 3. **Tempo Sync**

**JUCE:** Host provides tempo via `PlayHead::getPosition()`
```cpp
// JUCE version
double getTempo() const {
    if (auto pos = getPlayHead()->getPosition())
        if (auto bpm = pos->getBpm())
            return bpm.value();
    return 120.0;
}
```

**Teensy:** Local tempo knob or external clock input
```cpp
// Teensy version
double globalTempo = 120.0;  // From knob input

void updateTempoKnob() {
    int analogValue = analogRead(TEMPO_PIN);
    globalTempo = map(analogValue, 0, 1023, 40, 200);  // 40-200 BPM
}
```

### 4. **Parameter Changes**

**JUCE:** Host sends parameter updates via `AudioProcessorValueTreeState`
```cpp
// All parameter changes go through atomics in DSP modules
dsp::freezeEffect_.setDryWetMix(parameterValue);
```

**Teensy:** Local UI controls
```cpp
// Teensy version
void updateUI() {
    // Read potentiometers, buttons, encoders
    freezeEffect_.setDryWetMix(readPotentiometer(DRY_WET_PIN) / 1023.0f);
    freezeEffect_.setFrozen(readButton(FREEZE_BUTTON));
}
```

### 5. **Per-Sample Gain**

The DSP layer now supports per-pad volume via `SamplerBank::setSampleGain(index, gain)`.
In JUCE this is driven by the APVTS `sampleGain0-3` parameters; on Teensy it will be
driven by a potentiometer whose target pad is selected by the sample-select button.

```cpp
// Teensy version
void updatePerSampleControls() {
    // currentSampleTab is toggled by the sample-select button (0-3)
    float gainValue = analogRead(SAMPLE_GAIN_POT) / 1023.0f * 2.0f;  // 0.0-2.0
    samplerBank_.setSampleGain(currentSampleTab, gainValue);
}
```

## Shared DSP Core

All these files stay **exactly the same** for Teensy:

```
src/dsp/
├── CircularBuffer.h/cpp       ✓ No changes
├── Sampler.h/cpp              ✓ No changes (includes per-pad gain)
├── SamplerBank.h/cpp          ✓ No changes (setSampleGain forwarding)
├── Mixer.h/cpp                ✓ No changes
├── FreezeEffect.h/cpp         ✓ No changes (supports 1/32 + 1/64 stutter)
└── AudioBuffer.h/cpp          ✓ No changes
```

All DSP code is pure C++17. No JUCE headers, no virtual calls, no heap allocation
in real-time paths. `std::atomic<>` is used for parameter changes—this compiles
on Teensy 4.1 (ARM Cortex-M7 supports lock-free atomics for 32-bit types).

## Teensy-Specific Code Location

Create these new files in `src/hardware/teensy/`:

```
src/hardware/teensy/
├── I2SCodecDriver.h/cpp       # Audio codec interfacing
├── MidiInput.h/cpp            # USB/Serial MIDI handler
├── UIController.h/cpp         # UI knobs, buttons, display
├── TempoController.h/cpp      # Tempo sync logic
└── TeensySampler.cpp          # main() and setup()
```

## Planned Teensy Hardware Layout

The Teensy version uses physical controls instead of a GUI window.
All DSP calls are identical; only the control source changes.

### Buttons (digital pins)
| Button | Function |
|--------|----------|
| Trigger 1–4 | Trigger sample pads 0-3 (replaces MIDI note-on) |
| Freeze | Toggle freeze on/off |
| Series/Parallel | Toggle mixer routing mode |
| Sample Select | Cycle through pads 0-3 for per-sample controls |

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

The "Sample Select" button selects which pad the per-sample pots (gain, start, end)
currently control. An LED or small display indicates the active pad (1-4).

### Stutter Rate Mapping
The stutter pot maps to 7 discrete values (pot range divided into 7 zones):
```cpp
static constexpr double kStutterValues[7] = {
    1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.015625
};  // 1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64 beat

int zone = analogRead(STUTTER_POT) * 7 / 1024;
freezeEffect_.setStutterFraction(kStutterValues[zone]);
```

## Minimal Teensy Example

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

## Memory Considerations

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

## Sample Loading: The One Real Divergence

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

## Testing Strategy

1. **Compile DSP core independently** on Teensy to verify no JUCE dependencies:
   ```bash
   // Create minimal Teensy sketch that compiles src/dsp/ without errors
   ```

2. **Stub out I/O first**: Create dummy functions for audio I/O and MIDI before full integration

3. **Validate DSP output**: Use Teensy audio library or external DAC to monitor output signal

4. **Integrate incrementally**: Add UI controls, preset storage, then advanced features

## Porting Checklist

- [ ] Copy `src/dsp/` to Teensy project
- [ ] Verify compilation with Teensy-specific includes only
- [ ] Implement `AudioCodecDriver` (I2S interfacing)
- [ ] Implement `MidiInput` (USB MIDI handling)
- [ ] Create minimal `main()` loop
- [ ] Test MIDI triggering
- [ ] Test tempo sync with hardware tempo input
- [ ] Integrate UI controls (4 trigger buttons + sample-select button)
- [ ] Wire freeze pots (stutter, speed, dry/wet, loop start/end)
- [ ] Wire per-sample pots (gain, start, end) + sample-select cycling
- [ ] Wire mixer controls (parallel toggle, input/sampler level)
- [ ] Implement sample loading from SD card
- [ ] Add preset storage (SD card)
- [ ] Performance profiling and optimization

## Debugging Tips

- Use `Serial.print()` for logging in audio callback (careful: may cause glitches)
- Monitor CPU load with `AudioProcessorUsage`
- Use external analyzer to verify audio output

See `src/hardware/teensy/` for full implementation when ready.
