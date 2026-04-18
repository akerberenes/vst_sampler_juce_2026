#pragma once

#include "Sampler.h"
#include <array>

/**
 * SamplerBank
 *
 * Manages 4 independent Sampler instances and sums their audio output.
 *
 * --- Concept ---
 * Think of this as a 4-pad drum machine (like 4 pads on an MPC).
 * Each pad holds a different audio file; MIDI notes trigger the pads.
 * All pads can play simultaneously and their audio is mixed together.
 *
 * --- Level management ---
 * Each pad contributes at 25% (0.25) of full scale before the mixLevel multiplier.
 * This means all 4 pads playing at full volume simultaneously sum to 100%,
 * preventing clipping without needing a compressor or limiter.
 *
 * --- MIDI mapping ---
 * The MIDI-to-pad mapping is handled by PluginProcessor (not SamplerBank).
 * SamplerBank receives a 0-based index (0–3) and doesn't know about MIDI notes.
 *
 * No JUCE dependencies — pure C++17.
 */
class SamplerBank
{
public:
    // The number of pads in this bank. Compile-time constant so stack allocation
    // works for the samplers_ array.
    static constexpr int NUM_SAMPLES = 4;

    SamplerBank();
    ~SamplerBank() = default;

    // --- Configuration ---

    // Propagate the sample rate to all 4 Sampler instances.
    // Call this in prepareToPlay() so beat-to-sample conversion is accurate.
    void setSampleRate(int sr);

    // Convenience wrapper used by PluginProcessor::prepareToPlay().
    // `maxBlockSize` is accepted for API completeness but not currently used.
    void prepare(int sampleRate, int maxBlockSize);

    // --- Direct sampler access ---

    // Return a reference to one of the 4 Sampler objects (index 0–3).
    // Allows the caller to call stop() or query isPlaying() on a specific pad
    // (used by handleMidiNoteOff in PluginProcessor).
    // Out-of-range indices are clamped to 0.
    Sampler& getSample(int index);
    const Sampler& getSample(int index) const;

    // --- Playback ---

    // Trigger pad `noteIndex` (0–3) to start playing.
    // `beatDuration` and `tempoInBPM` are passed through to Sampler::trigger().
    // Out-of-range noteIndex is silently ignored.
    void triggerSample(int noteIndex, double beatDuration, double tempoInBPM);

    // Stop all 4 pads immediately.
    void stopAll();

    // --- Audio generation ---

    // Sum the output of all currently-playing pads into `outAudio`.
    // Non-playing pads contribute nothing (their processBlock returns silence).
    // Each pad is attenuated by 0.25 × `mixLevel` to prevent clipping.
    // `tempoInBPM` is passed through to each sampler for beat-duration tracking.
    void processBlock(float* outAudio, int numSamples, double tempoInBPM, float mixLevel = 1.0f);

    // --- Sample loading ---

    // Copy audio data into pad slot `noteIndex`.
    // Thread-safe to call from the main thread between audio blocks.
    void loadSampleData(int noteIndex, const float* audioData, int lengthInSamples);

    // Unload and stop pad slot `noteIndex`.
    void clearSampleData(int noteIndex);

    // --- Per-pad region control ---

    // Set loop mode on pad `noteIndex` (see Sampler::setLoopMode).
    void setSampleLoopMode(int noteIndex, bool enable);

    // Set the playback start point (0.0–1.0) for pad `noteIndex`.
    void setSampleStartFraction(int noteIndex, float fraction);

    // Set the playback end point (0.0–1.0) for pad `noteIndex`.
    void setSampleEndFraction(int noteIndex, float fraction);

private:
    // Fixed-size array of 4 Sampler objects.
    // std::array is preferred over std::vector when the size is a compile-time constant
    // because it avoids heap allocation and is trivially destructed.
    std::array<Sampler, NUM_SAMPLES> samplers_;

    // Current sample rate, mirrored to all samplers when setSampleRate() is called.
    int sampleRate_ = 48000;
};
