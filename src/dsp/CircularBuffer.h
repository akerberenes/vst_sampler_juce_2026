#pragma once

#include <vector>
#include <atomic>

/**
 * CircularBuffer
 *
 * A ring buffer ("tape loop") for continuous audio recording with freeze support.
 * Used internally by FreezeEffect as its capture and playback engine.
 *
 * --- How a ring buffer works ---
 * Imagine a physical tape loop: as the tape plays, a write head records new audio
 * and a separate read head plays back audio from a slightly earlier position.
 * When either head reaches the physical end of the tape, it wraps to the start.
 *
 * In memory:
 *   index: [  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11 ]
 *                        ↑ readPos                  ↑ writePos
 *   Both positions advance forward and wrap at buffer_.size().
 *
 * --- Freeze behaviour ---
 *   RECORDING (frozen=false): writePos advances each sample, continuously capturing.
 *   FROZEN    (frozen=true) : writePos stops. readPos loops within [loopStart, loopEnd].
 *   The result: when frozen, the buffer plays the same captured audio forever.
 *
 * --- Loop boundaries ---
 *   loopStartFraction_  →  loopEndFraction_  defines the active playback window.
 *   Shrinking this window creates shorter, faster stutter patterns.
 *   Example: start=0.0 end=0.25 → only the first quarter of the buffer loops.
 *
 * --- Playback speed / pitch ---
 *   readPos advances by `playbackSpeed_` per output sample.
 *   speed=1.0  → normal pitch
 *   speed=0.5  → half speed, one octave down
 *   speed=2.0  → double speed, one octave up
 *   Because speed can be fractional, readPos is stored as a double.
 *
 * --- Thread safety ---
 *   writePos_, readPos_, frozen_, and playbackSpeed_ are std::atomic<> so they can
 *   be read from the UI thread (e.g. for a fill-level meter) while the audio thread
 *   writes them, without introducing data races or needing a mutex.
 *
 * No JUCE dependencies — pure C++17.
 */
class CircularBuffer
{
public:
    // Constructor. Pass maxSizeInSamples > 0 to allocate immediately.
    // Passing 0 defers allocation to a later call to allocate().
    explicit CircularBuffer(int maxSizeInSamples = 0);
    ~CircularBuffer() = default;

    // Allocate (or reallocate) the ring buffer to `sizeInSamples` floats.
    // Resets all pointers to 0, zeros all samples, and clears the freeze flag.
    void allocate(int sizeInSamples);

    // Zero all samples and reset read/write positions and freeze state.
    void clear();

    // --- Writing (recording) ---

    // Write one sample at writePos and advance writePos by 1 (with wrap-around).
    // No-op when frozen — the tape is locked.
    void pushSample(float sample);

    // Write `numSamples` samples from `audio` into the ring buffer.
    // Calls pushSample() for each sample so freeze-gate applies per sample.
    void pushBlock(const float* audio, int numSamples);

    // --- Reading (playback) ---

    // Read the sample at the current (loop-adjusted) readPos.
    // Advance readPos by playbackSpeed_ (allows fractional / pitch-shifted speeds).
    // readPos wraps within the [loopStart, loopEnd] window.
    float pullSample();

    // Fill `outAudio` with `numSamples` calls to pullSample().
    void pullBlock(float* outAudio, int numSamples);

    // --- Freeze control ---

    // Freeze: stop writing, keep looping the read pointer over captured audio.
    void freeze();

    // Unfreeze: resume writing; new audio will overwrite old content.
    void unfreeze();

    // Returns true if currently frozen.
    bool isFrozen() const;

    // --- Loop boundary control ---
    // Fractions of total buffer length (0.0 = start, 1.0 = end).
    // Narrowing the window to e.g. [0.0, 0.25] creates a short stutter loop.

    // Set the start of the playback loop region.
    void setLoopStart(float fraction);

    // Set the end of the playback loop region.
    void setLoopEnd(float fraction);

    float getLoopStart() const { return loopStartFraction_; }
    float getLoopEnd() const { return loopEndFraction_; }

    // --- Playback speed (pitch shift) ---

    // Set how many samples readPos advances per output sample.
    // Clamped to >= 0.01 to prevent zero or reverse movement.
    void setPlaybackSpeed(float speed);

    float getPlaybackSpeed() const { return playbackSpeed_.load(); }

    // --- Read position ---

    // Jump the read pointer to `fraction` of the total buffer length.
    void setReadPosition(float fraction);

    // Returns current read position as a fraction of total buffer size (0.0–1.0).
    float getReadPosition() const;

    // --- Info ---
    int getSize() const { return buffer_.size(); }

    // Direct read-only access to the raw buffer data. Use carefully.
    const float* getBuffer() const { return buffer_.data(); }

private:
    // The ring buffer storage. All samples live here.
    std::vector<float> buffer_;

    // Index of the next sample to write. Wraps at buffer_.size().
    // Atomic: UI thread may read this to display a fill-level indicator.
    std::atomic<int> writePos_{0};

    // Exact read position in samples (fractional for sub-sample speed control).
    // Stored as double so fractional speeds accumulate without precision loss.
    std::atomic<double> readPos_{0.0};

    // Whether recording is paused (frozen = true).
    std::atomic<bool> frozen_{false};

    // How many samples readPos advances per output sample. 1.0 = normal pitch.
    std::atomic<float> playbackSpeed_{1.0f};

    // Loop region boundaries as fractions (not absolute indices).
    // Only written from the audio thread via parameter updates; no atomic needed.
    float loopStartFraction_ = 0.0f;
    float loopEndFraction_ = 1.0f;

    // Convert raw readPos_ into an absolute index guaranteed to fall inside
    // [loopStart, loopEnd) using modular arithmetic.
    double getLoopAdjustedReadPos() const;
};
