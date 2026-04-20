# Session Notes - VST Plugin Development

## Last Updated: April 20, 2026

## Project Overview
JUCE-based VST3/Standalone sampler plugin with echo-freeze effect, designed for Teensy 4.1 portability.
- 4-sample one-shot MIDI-triggered sampler with per-pad gain and effects
- Tempo-synced loop playback with fractional bar support
- Master echo-freeze effect with stutter/speed/loop manipulation
- Parallel/Sequential routing modes
- 5-page LCD menu system (TeensyMenu) with hardware-portable state machine
- XML preset system (8 slots, Save/Reload/LoadOther)

## Architecture Layers

### Tier 1: Foundation
- **AudioBuffer** — Generic multi-channel audio buffer for inter-module communication

### Tier 2: DSP Building Blocks (Pure C++17, no JUCE)
- **CircularBuffer** — Ring buffer for freeze effect (write/read pointers, speed control, loop boundaries)
- **Sampler** — Single sample playback with tempo sync, cached totalFrames_, per-pad gain
- **Mixer** — Routes audio based on parallel/sequential mode
- **SamplerBank** — Container for 4 independent Sampler instances

### Tier 2.5: Effects (Pure C++17, no JUCE)
- **Effect** — Abstract base class (virtual processSample)
- **NoEffect** — Passthrough (inlined)
- **Distortion** — fastTanh rational approximation (~28 cycles/sample on ARM)
- **BitCrush** — Sample quantisation (~31 cycles/sample)
- **SimpleFilter** — One-pole low-pass IIR (~14 cycles/sample)
- **EffectLibrary** — Static factory `createEffect(index)`, NUM_EFFECTS=4

### Tier 3: Orchestration
- **FreezeEffect** — Freeze state machine (recording → frozen → manipulate), stutter quantization (1/32 + 1/64), dry/wet
- **TeensyMenu** — 5-page LCD menu state machine (Sample 1–4, Preset)
  - 3 knob inputs (pageKnob, paramKnob, paramValue) + action button
  - Per-sample effect selection via EffectLibrary
  - 3-way preset function (Save/Reload/LoadOther) with destination pickup guard
  - Dirty tracking with * indicator
  - Pickup mode for smooth parameter transitions

### Tier 4: JUCE Integration
- **PluginProcessor** — AudioProcessor hub (MIDI, APVTS params, DSP chain, XML preset I/O)
  - Member buffers (std::vector<float>) instead of stack arrays
  - Cached param ID strings (sampleParamIds_[4])
  - Creates Preset1.xml + Preset2.xml on first launch
- **PluginEditor** — 750×800 window (SampleTabPanel + freeze controls + mixer + TeensyEmulationPanel)
- **SampleTabPanel** — Tabbed per-sample editor (waveform, markers, gain, effect, obey note off)
- **WaveformDisplay** — Interactive waveform with draggable green(start)/red(end) markers
- **TeensyEmulationPanel** — LCD display + 3 knobs + action button (emulates hardware UI)
- **FreezeBufferDisplay** — Real-time circular buffer waveform

## Signal Flow

```
Audio Input (from DAW)
    ↓
SamplerBank (4 samplers, each with per-pad effect + gain)
    ↓
Mixer (Sequential: blend input+samplers | Parallel: samplers only)
    ↓
FreezeEffect (record/freeze/playback with stutter, speed, loop control)
    ↓
Output (to DAW)
```

## Build & Test

### Build
```bash
cmake --build C:\Users\anker\scripts\music_projects\vst_plugin_project\build --config Release
```

### Test
```bash
.\build\unit_tests\Release\DSPTests.exe    # 56 tests, 251 assertions
```

### VST3 Output
```
build\SamplerWithFreeze_artefacts\Release\VST3\Sampler With Freeze.vst3
→ Post-build copies to C:\JUCE\plugins\vst\
```

### Preset Files
```
%APPDATA%\SamplerWithFreeze\Presets\Preset1.xml through Preset8.xml
```

## Key Design Decisions

### Real-Time Safety
- **Atomics, not locks** — Audio callback uses `std::atomic` for parameter changes
- **Member buffers** — `std::vector<float>` allocated once in prepareToPlay()
- **No dynamic allocation** in audio thread (processBlock)
- **Cached param IDs** — no string construction in audio thread

### Teensy Portability
- All DSP modules (`src/dsp/`) are pure C++17 with zero JUCE deps
- Virtual dispatch for effects: ~6 cycles, acceptable
- std::string/std::function only in TeensyMenu (UI thread)
- **No portability blockers found** (audit April 20, 2026)
- **2.8% CPU utilization** (97%+ headroom on Teensy 4.1 @ 600MHz)

### Effects System
- Abstract base `Effect` with virtual `processSample(float)` and `setParameter(float)`
- Each Sampler owns a raw `Effect*` pointer; TeensyMenu owns `unique_ptr<Effect>`
- Effect is applied per-sample in Sampler::interpolateSample()
- fastTanh in Distortion: `x*(27+x²)/(27+9x²)` with ±3.0 clamp

### Preset System
- XML files at AppData/SamplerWithFreeze/Presets/
- 3-way function: Save (writes current APVTS to XML), Reload (reloads current), LoadOther (browse + load)
- Destination pickup guard: when switching to LoadOther, knob must "find" current position before changing
- Dirty flag set via valueTreePropertyChanged(), displayed as * prefix on LCD

## Dependencies
- **JUCE** (C:/JUCE) — VST3, Standalone generation, GUI
- **Catch2 v3.4.0** (amalgamated, in unit_tests/catch2/) — Unit testing
- **C++17 / CMake 3.24+ / Visual Studio 17 2022 x64**

## Development Roadmap

### ✓ Phase 1: Core DSP (April 18)
### ✓ Phase 2: JUCE Integration (April 19)
### ✓ Phase 3: Effects & Menu System (April 19)
### ✓ Phase 4: Preset System (April 19-20)
### ✓ Phase 5: Refactoring & Optimization (April 20)
### → Phase 6: Polish & Testing (In Progress)
- DAW testing in Reaper
- Audio file format polish
- Potential: more effects, higher-order filters

### Teensy Port (Future)
- Phase A: Compile gate (PlatformIO)
- Phase B: DSP validation (Catch2 baseline)
- Phase C: Control logic simulation (Wokwi)
- Phase D: Sample loading (Wokwi + SD)
- Phase E: Hardware audio (I2S + USB MIDI)
- See docs/PORTING.md

## Important Notes for Future Sessions
- **Always consider Teensy portability** when modifying `src/dsp/` — no JUCE deps, no heap in audio thread
- **User prefers manual git pushes** — never auto-push
- **Backup before major changes** — use backup.ps1
- **Preset names**: "Preset1", "Preset2", etc. (not "P1" or "Default")
- **Test after every change**: 56 tests must all pass before deploying
- If Reaper has the .vst3 locked, close Reaper before rebuilding
