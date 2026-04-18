#pragma once

#include "CircularBuffer.h"
#include <atomic>

/**
 * FreezeEffect
 *
 * Master echo-freeze effect. This is the final processing stage before output.
 *
 * --- What it does ---
 * FreezeEffect wraps a CircularBuffer and adds the high-level control layer:
 *   - Continuously captures incoming audio into the ring buffer (Recording state).
 *   - On "freeze": stops writing, loops the captured content indefinitely (Frozen state).
 *   - Stutter: on each beat subdivision, jumps the read pointer back rhythmically.
 *   - Speed: scales how fast the read pointer advances (pitch shift).
 *   - Dry/wet: blends the original (dry) signal with the frozen (wet) signal.
 *
 * --- Stutter mechanism ---
 * The stutter is driven by a simple sample accumulator:
 *   stutterAccumulator_ += blockSize every processBlock call.
 *   When accumulator >= stutterSamples (derived from BPM + stutterFraction),
 *   the read pointer is jumped forward by stutterFraction of the loop region.
 *   This creates a rhythmic "stutter" aligned to the project tempo.
 *
 * --- Dry/wet crossfade ---
 *   dryWet = 0.0  → 100% dry (no effect audible)
 *   dryWet = 1.0  → 100% wet (fully frozen audio, no original signal)
 *   In between: linear blend.
 *
 * --- Recording mode behaviour ---
 * Even when not frozen, FreezeEffect passes audio through unchanged (dry output).
 * It continuously fills the buffer so that when the user hits freeze, the buffer
 * already contains the most recent audio.
 *
 * No JUCE dependencies — pure C++17.
 */
class FreezeEffect
{
public:
    // Processing state: either capturing new audio or looping captured audio.
    enum class State : uint8_t
    {
        Recording = 0,   // Buffer is being written; output is dry (pass-through).
        Frozen    = 1    // Buffer is locked; output is the frozen loop.
    };

    // `maxBufferSizeInSamples`: pre-allocate the ring buffer. Pass 0 to defer.
    // Normally, prepare() allocates a sensible default based on sample rate.
    explicit FreezeEffect(int maxBufferSizeInSamples = 0);
    ~FreezeEffect() = default;

    // --- Setup ---

    // Call from prepareToPlay(). Stores sample rate and allocates the ring buffer
    // to a default size of 4 seconds (enough for ~2 bars at 120 BPM).
    void prepare(int sampleRate, int maxBlockSize);

    // Override the buffer size in samples (advanced use).
    void setBufferSizeInSamples(int size);

    // Returns the current ring buffer capacity in samples.
    int getBufferSizeInSamples() const { return freezeBuffer_.getSize(); }

    // --- Freeze control ---

    // Freeze (true) or unfreeze (false) the buffer.
    // Freezing: stops writing, read pointer starts looping over captured audio.
    // Unfreezing: resumes writing; old content is gradually overwritten.
    void setFrozen(bool b);

    // Returns true when the buffer is frozen.
    bool isFrozen() const { return freezeBuffer_.isFrozen(); }

    // --- Audio processing ---

    // The main per-block processing function.
    // `inputAudio`  : mixed audio from the Mixer (what gets captured/passed through).
    // `outputAudio` : destination buffer for the processed result.
    // `numSamples`  : number of samples to process (must be <= 4096).
    // `tempoInBPM`  : current project BPM, used to time the stutter jump interval.
    void processBlock(const float* inputAudio, float* outputAudio, int numSamples,
                      double tempoInBPM);

    // --- Stutter ---

    // Set the stutter interval as a fraction of one beat.
    // 1.0   = jump once per beat  (slowest stutter)
    // 0.5   = jump twice per beat
    // 0.25  = four times per beat
    // 0.125 = eight times per beat (default — tight stutter)
    // 0.0625= sixteen times per beat (fastest stutter)
    void setStutterFraction(double beatFraction) { stutterFraction_ = beatFraction; }
    double getStutterFraction() const { return stutterFraction_; }

    // --- Playback speed (pitch) ---

    // Scales how fast the frozen loop plays back.
    // 1.0 = original pitch, 0.5 = one octave down, 2.0 = one octave up.
    void setPlaybackSpeed(float speed) { freezeBuffer_.setPlaybackSpeed(speed); }
    float getPlaybackSpeed() const { return freezeBuffer_.getPlaybackSpeed(); }

    // --- Loop boundaries ---

    // Set the start/end of the frozen loop as a fraction of the buffer (0.0–1.0).
    // Narrowing the window creates shorter loop patterns.
    void setLoopStart(float fraction) { freezeBuffer_.setLoopStart(fraction); }
    void setLoopEnd(float fraction)   { freezeBuffer_.setLoopEnd(fraction); }
    float getLoopStart() const        { return freezeBuffer_.getLoopStart(); }
    float getLoopEnd()   const        { return freezeBuffer_.getLoopEnd(); }

    // --- Dry/wet mix ---

    // 0.0 = fully dry (original audio, no freeze audible).
    // 1.0 = fully wet (only the frozen loop is heard).
    void setDryWetMix(float wet);  // Clamped to [0.0, 1.0]
    float getDryWetMix() const { return dryWetMix_.load(); }

    // --- Debug ---

    // Returns the read pointer's current position in the buffer as a percentage (0–100%).
    // Useful for visualising how full the buffer is.
    float getBufferFillPercentage() const;

    // Returns the current state (Recording or Frozen).
    State getState() const { return static_cast<State>(state_.load()); }

private:
    // The underlying ring buffer that captures and replays audio.
    CircularBuffer freezeBuffer_;

    // Sample rate in Hz, set by prepare().
    int sampleRate_ = 48000;

    // Current state (Recording or Frozen). Redundant with freezeBuffer_.isFrozen()
    // but kept for fast state queries without going through the buffer.
    std::atomic<uint8_t> state_{static_cast<uint8_t>(State::Recording)};

    // Dry/wet blend factor (0.0 = dry, 1.0 = wet). Atomic for UI-thread safety.
    std::atomic<float> dryWetMix_{1.0f};

    // Stutter interval as a fraction of a beat (e.g. 0.125 = 1/8 beat).
    double stutterFraction_ = 0.125;

    // Accumulates block sizes; when >= stutterSamples, triggers a read-pointer jump.
    double stutterAccumulator_ = 0.0;

    // Calculate when the next stutter jump should occur and execute it if due.
    // Called once per processBlock when the buffer is frozen.
    void updateStutterPlayback(double tempoInBPM, int blockSize);
};
