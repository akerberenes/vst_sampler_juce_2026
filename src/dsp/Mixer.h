#pragma once

#include <atomic>

/**
 * Mixer
 *
 * Routes audio to the FreezeEffect based on the user's selected mode.
 *
 * --- Sequential mode (default) ---
 * The incoming DAW/mic audio and the sampler output are blended together
 * before reaching the freeze effect. The freeze captures both.
 *
 *   input  × inputLevel  ─┬────────────────────┐
 *                          │ sum → FreezeEffect → output
 *   sampler × samplerLevel ─┘
 *
 *   Use case: freeze your voice + the sample loop together.
 *
 * --- Parallel mode ---
 * Only the sampler output goes to the freeze effect.
 * The DAW/mic input bypasses the freeze and is added back in
 * by PluginProcessor after the freeze returns.
 *
 *   input  ───────────────────────────────────────────────┐
 *                                                              │ → output
 *   sampler × samplerLevel ───────────┐                        │
 *                            │ FreezeEffect → frozen audio ─┘
 *
 *   Use case: freeze only the sample loops; keep playing guitar live over them.
 *
 * No JUCE dependencies — pure C++17.
 */
class Mixer
{
public:
    // Routing mode. Stored as uint8_t in the atomic to avoid the overhead
    // of an atomic<enum> which may not be lock-free on all platforms.
    enum class Mode : uint8_t
    {
        Sequential = 0,  // input + sampler mixed → freeze
        Parallel   = 1   // sampler only → freeze; input bypasses
    };

    Mixer();
    ~Mixer() = default;

    // --- Mode control ---

    // Switch between Sequential and Parallel routing.
    void setMode(Mode m) { mode_.store(static_cast<uint8_t>(m)); }
    Mode getMode() const { return static_cast<Mode>(mode_.load()); }

    // Convenience query functions.
    bool isParallelMode()   const { return getMode() == Mode::Parallel; }
    bool isSequentialMode() const { return getMode() == Mode::Sequential; }

    // --- Level controls ---

    // Set the gain applied to the DAW/mic input signal before mixing (0.0–1.0).
    // In parallel mode this has no effect on the freeze path (input bypasses),
    // but PluginProcessor uses it when adding the input back to the final output.
    void setInputLevel(float level);   // Clamped to [0.0, 1.0]

    // Set the gain applied to the sampler output before mixing (0.0–1.0).
    void setSamplerLevel(float level); // Clamped to [0.0, 1.0]

    float getInputLevel()   const { return inputLevel_.load(); }
    float getSamplerLevel() const { return samplerLevel_.load(); }

    // --- Audio processing ---

    // Mix `inputAudio` and `samplerAudio` into `outputAudio` according to the current mode.
    // Sequential: output[i] = input[i] * inputLevel + sampler[i] * samplerLevel
    // Parallel:   output[i] = sampler[i] * samplerLevel  (input is NOT included here;
    //             PluginProcessor adds it back to the final output after the freeze step)
    // Either input pointer may be null; that channel is treated as silence.
    void processBlock(const float* inputAudio, const float* samplerAudio,
                      float* outputAudio, int numSamples);

private:
    // Current routing mode. Atomic so the UI thread can toggle it safely.
    std::atomic<uint8_t> mode_{static_cast<uint8_t>(Mode::Sequential)};

    // Per-source gain factors. Atomic for UI-thread write safety.
    std::atomic<float> inputLevel_{1.0f};
    std::atomic<float> samplerLevel_{1.0f};
};
