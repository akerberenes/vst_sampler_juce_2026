# Session Notes - VST Plugin Development

## Project Overview
Building a JUCE-based VST3/AU plugin (with Teensy hardware portability) featuring:
- 4-sample one-shot MIDI-triggered sampler
- Tempo-synced loop playback with fractional bar support
- Master echo-freeze effect with stutter/speed/loop manipulation
- Parallel/Sequential routing modes
- Preset system with embedded samples

## Architecture Layers

### Tier 1: Foundation
- **AudioBuffer** — Generic multi-channel audio buffer for inter-module communication

### Tier 2: DSP Building Blocks (Pure C++, no JUCE)
- **CircularBuffer** — Ring buffer for freeze effect (write pointer, read pointer with loop boundaries, speed control)
- **Sampler** — Single sample playback with tempo sync (trigger by beat duration, linear interpolation, loop fractions)
- **Mixer** — Routes audio based on parallel/sequential mode
- **SamplerBank** — Container for 4 independent Sampler instances

### Tier 3: Orchestration
- **FreezeEffect** — Coordinates freeze state machine (recording → frozen → manipulate), stutter quantization, dry/wet mixing

### Tier 4: JUCE Integration
- **PluginProcessor** — Main AudioProcessor (MIDI handling, host tempo query, audio chain coordination)
- **PluginEditor** — Minimal MVP UI (load buttons for samples)

## Signal Flow

```
Audio Input (from DAW)
    ↓
SamplerBank (4 samplers, each triggered by MIDI)
    ↓
Mixer (Sequential: blend input+samplers | Parallel: samplers only)
    ↓
FreezeEffect (record/freeze/playback with stutter, speed, loop control)
    ↓
Output (to DAW)
```

## Key Design Decisions

### Real-Time Safety
- **Atomics, not locks** — Audio callback uses `std::atomic` for parameter changes
- **Stack buffers** — `float tempBuffer[4096]` allocated once per callback
- **No dynamic allocation** in audio thread (processBlock)

### Freezing Mechanics
- CircularBuffer uses two independent pointers: write (recording) and read (playback)
- When frozen: stop writing new samples, read pointer loops the captured buffer
- Loop boundaries (start/end fraction) allow extending/shortening the frozen loop

### Sampler Tempo Sync
- `trigger(beatDuration, tempoInBPM)` calculates duration in samples
- Example: 4 beats at 120 BPM = 2 seconds = 96,000 samples @ 48kHz
- Loop fractions allow repeating at partial sample length

### Routing Modes
- **Sequential (default)** — Input mixed with sampler output, both captured by freeze
- **Parallel** — Input isolated, only sampler output captured by freeze

### Stutter Implementation
- FreezeEffect accumulates time and jumps read pointer at rhythmic intervals
- Stutter rate tied to host BPM for perfect quantization

## Dependencies
- **JUCE** (VST3, AU, Standalone generation)
- **libsndfile** (WAV support)
- **libmpg123** (MP3 support)
- **FLAC C API** (FLAC support)
- **C++17 compiler** (MSVC, Clang++, or GCC)

## Build Steps

### 1. Install JUCE
```bash
git clone https://github.com/juce-framework/JUCE.git C:\JUCE
```

### 2. Build Project
```bash
cd C:\Users\anker\scripts\music_projects\vst_plugin_project
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### 3. Plugin Output
```
build/Release/SamplerWithFreeze_VST3/Release/SamplerWithFreeze.vst3
```

### 4. Install to Reaper
```
Copy .vst3 to C:\Program Files\REAPER\Plugins\VST3\
```

## Unit Testing Strategy

All DSP modules are purely numeric (no audio listening):
- **CircularBuffer tests:** Push/pull samples, freeze state, loop boundaries
- **Sampler tests:** Tempo sync duration calculation, loop fractions
- **Mixer tests:** Mode switching, input/sampler blending
- **FreezeEffect tests:** Stutter timing accuracy, state transitions

Test framework: Standalone executable or Catch2

## Development Roadmap

### Phase 1: ✓ Complete
- Project setup
- CMakeLists.txt
- DSP skeleton + implementations
- JUCE integration scaffold

### Phase 2: TODO
- Complete DSP implementations (verify logic)
- Unit tests for all modules
- Build and initial compilation

### Phase 3: TODO
- JUCE parameter management
- MIDI routing completion
- Test in DAW (Reaper)

### Phase 4: TODO
- Audio file loading (WAV, MP3, FLAC)
- Preset manager (save/load with embedded samples)
- Waveform display UI

### Phase 5: TODO
- Echo freeze full stutter/speed/loop implementation
- Integration testing

### Phase 6: TODO
- Teensy hardware adaptation (structure prep only)

### Phase 7: TODO
- Full testing, performance optimization, bug fixes

## Important Notes

### Real-Time Constraints
- No file I/O in `processBlock()`
- No memory allocation in audio thread
- No blocking calls or locks
- All state changes via atomics

### Polyphony Limitation
- One-shot playback only (each sample plays once per trigger)
- No overlapping instances of the same sample

### Freeze Buffer Sizing
- Default: 2 bars at 120 BPM = 4 seconds
- User-configurable up to 8 bars (~16 seconds)
- Limited by available PSRAM on Teensy

### Teensy Portability
- All DSP modules (src/dsp/) are pure C++ with zero JUCE deps
- Only JUCE-specific code: PluginProcessor, PluginEditor
- See docs/PORTING.md for Teensy adaptation strategy

## Current Project Status
- **Git Repository:** Initialized locally (not pushed to GitHub yet)
- **Compilation:** Not yet tested (JUCE required)
- **DAW Testing:** Pending
- **Platform:** Targeted for Windows (MSVC), macOS (Xcode), Linux (Ninja+Clang)

## Next Immediate Tasks
1. Install JUCE framework
2. Configure CMakeLists.txt to find JUCE installation
3. Build project and verify compilation
4. Run unit tests to validate DSP logic
5. Test plugin in Reaper
