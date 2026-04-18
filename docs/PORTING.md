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

## Shared DSP Core

All these files stay **exactly the same** for Teensy:

```
src/dsp/
├── CircularBuffer.h/cpp       ✓ No changes
├── Sampler.h/cpp              ✓ No changes
├── SamplerBank.h/cpp          ✓ No changes
├── Mixer.h/cpp                ✓ No changes
├── FreezeEffect.h/cpp         ✓ No changes
└── AudioBuffer.h/cpp          ✓ No changes
```

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

void handleMidiNoteOn(byte channel, byte note, byte velocity) {
    int sampleIndex = (note - 60) % 4;  // C3 = sample 0
    samplerBank.triggerSample(sampleIndex, 4.0, globalTempo);
}

void loop() {
    // Handle USB MIDI
    while (usbMIDI.read()) {}
    
    // Update UI (tempo pot, freeze button, etc)
    updateUI();
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
- [ ] Integrate UI controls
- [ ] Add preset storage (SD card)
- [ ] Performance profiling and optimization

## Debugging Tips

- Use `Serial.print()` for logging in audio callback (careful: may cause glitches)
- Monitor CPU load with `AudioProcessorUsage`
- Use external analyzer to verify audio output

See `src/hardware/teensy/` for full implementation when ready.
