# Sampler with Freeze - VST Plugin

A JUCE-based VST3/AU plugin and standalone sampler with an echo-freeze effect for creative audio manipulation.

## Features

- **4-Sample One-Shot Sampler** with tempo-synced looping
- **Echo Freeze Effect** on master output (captures input + samplers in sequential mode, or samplers only in parallel mode)
- **Stutter Control** with rhythmic quantization
- **Speed Manipulation** for pitch shifting
- **Parallel/Sequential Routing** for flexible signal flow
- **Preset System** with embedded sample storage (portable presets)

## Project Structure

```
vst_plugin_project/
├── src/
│   ├── dsp/              # Platform-agnostic DSP core (C++)
│   │   ├── CircularBuffer.h/cpp
│   │   ├── Sampler.h/cpp
│   │   ├── SamplerBank.h/cpp
│   │   ├── Mixer.h/cpp
│   │   ├── FreezeEffect.h/cpp
│   │   └── AudioBuffer.h/cpp
│   ├── juce/             # JUCE integration layer
│   │   ├── PluginProcessor.h/cpp
│   │   └── PluginEditor.h/cpp
│   ├── audio/            # File I/O and presets
│   │   ├── AudioFileLoader.h/cpp
│   │   └── PresetManager.h/cpp
│   └── hardware/         # Future Teensy port (structure only)
├── tests/                # Unit tests for DSP modules
├── docs/                 # Documentation and architecture guides
└── CMakeLists.txt        # Build configuration
```

## Build Instructions

### Requirements

- C++17 compiler (MSVC, Clang++, or GCC)
- JUCE framework (vendored as submodule or system install)
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

## Architecture

### Signal Flow (Sequential Mode)

```
Input Audio ──┐
              ├──→ [Mixer] ──→ [Freeze Buffer] ──→ [Stutter/Speed] ──→ DAW Out
Samplers ─────┘
```

### Signal Flow (Parallel Mode)

```
Input Audio ──→ [Output - Isolated]
Samplers ──┬──→ [Freeze Buffer] ──→ [Stutter/Speed] ──→ DAW Out
```

## Development Roadmap

### Phase 1: Completed ✓
- Project setup
- CMakeLists.txt configuration
- DSP core module structure

### Phase 2: In Progress
- Complete DSP module implementations
- Unit tests for CircularBuffer, Sampler, Mixer, FreezeEffect

### Phase 3: TODO
- Full JUCE integration (PluginProcessor, parameters, MIDI)
- Test in DAW

### Phase 4: TODO
- File loading (WAV, MP3, FLAC support)
- Preset system

### Phase 5: TODO
- Echo freeze stutter/speed manipulation

### Phase 6: TODO
- Teensy hardware adaptation (structure prep)

### Phase 7: TODO
- Full testing and performance tuning

## Usage (Once Complete)

1. Load plugin in compatible DAW
2. Map MIDI keys to samples (C3-C#3 for samples 0-3)
3. Trigger sample playback with MIDI notes
4. Enable freeze mode to capture mixed output
5. Adjust stutter rate, playback speed, loop boundaries
6. Toggle parallel/sequential mode to change routing
7. Save presets for future sessions

## Porting to Teensy

The DSP core (`src/dsp/`) is completely isolated from JUCE and can be recompiled for Teensy with minimal changes. See `PORTING.md` for details.

## License

(Add your license here)
