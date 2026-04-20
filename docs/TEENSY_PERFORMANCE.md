# Teensy 4.1 Performance Reference

## Quick Reference

| Metric | Value | Notes |
|--------|-------|-------|
| CPU | ARM Cortex-M7 @ 600 MHz | 1.2 GHz overclockable |
| RAM | 512 KB on-chip + 8 MB PSRAM | PSRAM slower (10-15 cycles latency) |
| Audio Buffer | 48 kHz, 256-sample blocks | ~5.33 ms per block |
| FPU | Yes, native float | ~2 cycles per multiply |
| Max Audio CPU | 70% of available | Leave 30% for MIDI, USB, I/O |

---

## Performance Budget Calculation

### Given: 48 kHz, 256-sample block
```
Block Duration:   256 / 48000 = 5.33 ms
Available Time:   5.33 ms (hard deadline)
Safe Budget:      70% × 5.33 ms = 3.73 ms
CPU Cycles:       3.73 ms × 600 MHz = 2,238,000 cycles
Per Sample:       2,238,000 / 256 = 8,742 cycles available

Minimum Headroom: 30% reserved for:
  - I/O interrupts
  - MIDI polling
  - USB communication
  - System overhead
```

### Example: How Many Samplers Can We Run?
```
4 Samplers × 47 cycles/sample = 188 cycles/sample
  (includes: interpolation ~8, gain ~2, virtual dispatch ~6, effect ~28 [Distortion])
SamplerBank mixing: ~16 cycles/sample
Mixer (sequential mode): ~8 cycles/sample
FreezeEffect (frozen, with stutter): ~31 cycles/sample
Total DSP per sample = 243 cycles

Available: 8,742 cycles/sample
Used: 243 cycles/sample (worst case with 4× BitCrush: 247)
Utilization: 2.8% ✓ (97%+ headroom remaining)
```

### Per-Effect Cycle Costs (updated 2026-04-20)
```
NoEffect:     ~0 cycles (inlined passthrough)
Distortion:  ~28 cycles (fastTanh rational approx, no std::tanh)
BitCrush:    ~31 cycles (std::round + division)
SimpleFilter: ~14 cycles (one-pole IIR, single multiply-accumulate)
```

---

## Cycle Costs (Teensy 4.1, Approximate)

### Arithmetic (FPU Native)
| Operation | Cycles | Notes |
|-----------|--------|-------|
| Float add (f32) | 1 | Pipelined |
| Float multiply (f32) | 2 | Pipelined |
| Float divide (f32) | 14 | DO NOT USE in inner loop |
| Float mod (f32) via algorithm | 20-30 | Avoid in real-time |
| Integer multiply | 3 | Slower than float! |
| Integer divide | 12 | Avoid in real-time |

### Memory Access
| Operation | Cycles | Notes |
|-----------|--------|-------|
| Read from on-chip RAM | 1-4 | L1 cache hit |
| Write to on-chip RAM | 1 | Pipelined |
| Read from PSRAM | 10-15 | Much slower, avoid in loops |
| Write to PSRAM | 10-15 | Avoid in real-time path |
| Cache miss penalty | +10-20 | Sequential access minimizes this |

### Math Functions (Approximate, from ARM CMSIS-DSP)
| Function | Cycles | Alternative Cost | Notes |
|----------|--------|------------------|-------|
| `std::sin(f)` | 50-100 | Fast sine ~15-25 | Never use std::sin in DSP loop |
| `std::cos(f)` | 50-100 | Fast cosine ~15-25 | Use lookup table or Chebyshev |
| `std::sqrt(f)` | 40-80 | Q-Newton ~25 | Avoid; use Q-format if possible |
| `std::pow(f, f)` | 100-200+ | Multiply by constant ~2 | DO NOT USE |
| `std::exp(f)` | 80-150 | Lookup table ~3-5 | Use tables for modulation |

### Control Flow
| Operation | Cycles | Notes |
|-----------|--------|-------|
| Branch (predicted correct) | 1 | Pipeline flush avoided |
| Branch (mispredicted) | 10-15 | Avoid data-dependent branches |
| Loop overhead | 1-3 | Tight loops are fast |
| Function call (inlined) | 0 | Compiler should inline audio functions |
| Virtual function call | 5-8 | Avoid in inner loop (indirect jump) |

### Real-Time Safe Operations
| Operation | Cycles | Safe in Audio? |
|-----------|--------|----------------|
| `std::atomic::load()` | ~10 | ✓ Yes, but minimize frequency |
| `std::atomic::store()` | ~10 | ✓ Yes, parameter updates only |
| `memcpy()` (aligned) | 0.5-1 per byte | ✓ Yes, DMA if large blocks |
| Pointer dereference | 1-2 | ✓ Yes, normal |

---

## Computation Cost Estimation Worksheet

### For New DSP Functions: Fill This Out

```
Function Name: _________________________________
Sample Rate: ____________  Block Size: __________
Called From: processBlock() / prepare() / UI thread

INSTRUCTION COUNT (estimate cycles per sample):

Basic Operations:
  - Multiplies: _____ × 2 cycles = _____ cycles
  - Adds: _____ × 1 cycle = _____ cycles
  - Divides: _____ × 14 cycles = _____ cycles (DANGER!)
  - Comparisons/branches: _____ × 3 cycles = _____ cycles

Control Flow:
  - Loop iterations: _____ × _____ (loop cost) = _____ cycles
  - Function calls: _____ × _____ cycles = _____ cycles

Memory Access:
  - Sequential reads: _____ × 2 cycles = _____ cycles
  - Random access: _____ × 10 cycles = _____ cycles

Math Functions:
  - Fast sin/cos: _____ × 20 cycles = _____ cycles
  - Sqrt: _____ × 30 cycles = _____ cycles
  - Other: _____ × _____ cycles = _____ cycles

TOTAL: _____ cycles per sample

At 48 kHz, 256-sample block:
  Cycles per block: _____ × 256 = _____ cycles
  Time per block: _____ / 600,000,000 = _____ µs
  CPU utilization: _____ / 5,330 µs = ______ %

✓ SAFE if < 70% at 48 kHz (scale for other rates)
⚠ CAUTION if 70-85%
✗ NOT SAFE if > 85%
```

---

## Audio Function Cycle Budget Template

When designing a new audio function, start here:

```cpp
// Example: Sampler playback with linear interpolation
//
// Computational Profile:
// - Per sample: ~12 cycles (2 multiplies + 3 additions + interpolation logic)
// - At 48 kHz: 12 × 48,000 = 576,000 cycles/sec
// - Per 256-sample block: 12 × 256 = 3,072 cycles
// - Utilization @ 600 MHz: 3,072 / (5.33ms × 600M) = 0.096% ✓
//
// Bottleneck: Linear interpolation (2 multiplies per sample)
// Optimization potential: Use fixed-point math if needed
// Teensy safe: YES - plenty of headroom for 4 samplers + effects

float Sampler::interpolateSample(double position) const
{
    // Fractional lookup (3 operations: 2 × mult, 1 × add)
    int index = static_cast<int>(position);
    float frac = static_cast<float>(position - index);
    
    // Boundary check (1 cycle)
    if (index < 0 || index + 1 >= sampleData_.size())
        return 0.0f;
    
    // Linear interpolation: (1-frac)*s[i] + frac*s[i+1]
    // = s[i] + frac*(s[i+1] - s[i])
    // 5 operations: 2 × load, 1 × sub, 1 × mult, 1 × add
    float s0 = sampleData_[index];
    float s1 = sampleData_[index + 1];
    return s0 + frac * (s1 - s0);
}
```

---

## Performance Profiling on Teensy

### Method 1: Cycle Counter (Most Accurate)

```cpp
// In FreezeEffect.cpp:

#if defined(TEENSY) || defined(TEENSYDUINO)
    #include <core_pins.h>
    
    // ARM DWT (Data Watchpoint and Trace) cycle counter
    volatile unsigned int *DWT_CYCCNT = (unsigned int *)0xE0001004;
    volatile unsigned int *DWT_CONTROL = (unsigned int *)0xE0001000;
    volatile unsigned int *SCB_DEMCR = (unsigned int *)0xE000EDFC;
    
    inline void enableCycleCounter() {
        *SCB_DEMCR = *SCB_DEMCR | 0x01000000;
        *DWT_CONTROL = *DWT_CONTROL | 1;
    }
    
    inline unsigned int getCycleCount() {
        return *DWT_CYCCNT;
    }
#endif

void FreezeEffect::processBlock(const float* input, float* output, 
                                 int numSamples, double tempo)
{
    #ifdef TEENSY
        unsigned int startCycle = getCycleCount();
    #endif
    
    // ... actual processing ...
    
    #ifdef TEENSY
        unsigned int endCycle = getCycleCount();
        unsigned int cyclesUsed = endCycle - startCycle;
        unsigned int microseconds = cyclesUsed / 600;  // @ 600 MHz
        unsigned int percentageUsed = (cyclesUsed * 100) / (numSamples * 600000 / 48000);
        
        Serial.print("processBlock: ");
        Serial.print(microseconds);
        Serial.print(" µs (");
        Serial.print(percentageUsed);
        Serial.println("%)");
    #endif
}
```

### Method 2: Built-in Teensy Audio Library Profiler

```cpp
// Use AudioProcessorUsage() from Teensy Audio Library
#include <Audio.h>

void loop() {
    Serial.print("CPU: ");
    Serial.print(AudioProcessorUsage(), 1);
    Serial.print("% - ");
    Serial.print(AudioProcessorUsageMax(), 1);
    Serial.println("%");
    delay(1000);
}
```

### Method 3: VST Plugin in Reaper (for quick estimation)

1. Load plugin in Reaper
2. Enable FX window CPU meter (right-click FX → Options → Show CPU Display)
3. Look for sustained CPU % when playing audio + triggering effects
4. Target: < 5% on typical host system

---

## Common Performance Pitfalls

| Pitfall | Cost | Fix |
|---------|------|-----|
| `std::sin()` in loop | 50-100 cycles | Use lookup table or approximation |
| `std::vector::push_back()` | Alloc + ~50 cycles | Pre-allocate in prepare() |
| `std::mutex` in real-time | 50-200 cycles | Use `std::atomic` instead |
| `if (index < 0)` branch | Misprediction penalty | Use conditional move or branchless math |
| Nested loops over samples | N² scaling | Flatten loops, restructure algorithm |
| PSRAM access in inner loop | 10-15 cycles each | Cache in on-chip RAM first |
| `memcpy()` with unaligned data | 2-3 cycles per byte | Ensure 4-byte alignment |
| Exception handling in audio | 100-500 cycles | Use error codes instead |
| Virtual function in loop | 5-8 cycles overhead | Inline or use static dispatch |
| Data cache miss | +10-20 cycles | Sequential memory access |

---

## Optimization Priority List

For Teensy 4.1, optimize in this order:

1. **Avoid memory allocation** (saves ~1000+ cycles vs alloc cost)
2. **Reduce function calls** in inner loop (save ~5-10 cycles each)
3. **Replace expensive math** (std::sin → approx, saves ~50-80 cycles)
4. **Optimize memory layout** for cache hits (sequential beats random)
5. **Use SIMD** if processing >1 sample/operation (ARM NEON, not yet used)
6. **Restructure loops** to reduce branches (pipeline-friendly)
7. **Fine-tune arithmetic** - use fixed-point if float is overkill

---

## Target CPU Budgets (By Component)

```
Per-Block Time Budget @ 48 kHz, 256 samples, 600 MHz:

Total Available:             5.33 ms = 3,198,000 cycles
Audio DSP Budget (70%):      2,238,600 cycles

Breakdown:
├─ Sampler (4 × 120 cyc + gain):    123,904 cycles (5.1%)
├─ CircularBuffer read:             5,120 cycles (0.2%)
├─ Mixer:                           2,048 cycles (0.1%)
├─ FreezeEffect orchestration:      20,480 cycles (0.9%)
├─ Linear interpolation:            10,240 cycles (0.5%)
├─ Stutter calculations:            20,480 cycles (0.9%)
└─ Margin (for future growth):      2,056,328 cycles (92%) ✓✓✓
```

All current components use **only ~7.7% of available audio CPU.**
The new per-pad gain feature adds < 0.1% CPU overhead (8 cycles/sample).
The additional 1/32 and 1/64 stutter rates add zero extra cost—
the stutter calculation runs once per block regardless of the fraction value.

This leaves massive room for:
- Additional effects (reverb, delay, filtering)
- Complex modulation
- Polyphonic samplers
- Real-time waveform analysis

---

## References

- [ARM Cortex-M7 Generic User Guide](https://developer.arm.com/documentation/100269/0004/)
- [Teensy 4.1 Specs](https://www.pjrc.com/store/teensy41.html)
- [ARM CMSIS-DSP Benchmarks](https://github.com/ARM-software/CMSIS-DSP)
- [Real-Time Audio Programming 101](https://www.rossbencina.com/code/real-time-audio-programming-101/)
