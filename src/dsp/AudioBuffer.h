#pragma once

#include <vector>
#include <cstring>

/**
 * AudioBuffer
 *
 * A simple multi-channel audio buffer used to pass audio between DSP modules.
 *
 * --- Why not just use std::vector<float> directly? ---
 * Most DSP functions expect a raw float* per channel (e.g. for SIMD, or to match
 * hardware APIs). This class lays out all channel data in one flat allocation and
 * keeps pre-computed per-channel pointers so callers never compute offsets.
 *
 * Memory layout example (numChannels=2, numFrames=4):
 *   data_: [ L0, L1, L2, L3,  R0, R1, R2, R3 ]
 *            <-- channel 0 --> <-- channel 1 -->
 *   writePointers_[0] → &data_[0]  (left channel)
 *   writePointers_[1] → &data_[4]  (right channel)
 *
 * No JUCE dependencies — pure C++17.
 */
class AudioBuffer
{
public:
    // Default constructor: creates an empty, unallocated buffer.
    // Call allocate() before using any pointer accessors.
    AudioBuffer() = default;

    // Convenience constructor: allocates immediately with the given dimensions.
    explicit AudioBuffer(int numChannels, int numFrames);

    // --- Allocation ---

    // Resize the buffer to hold `numChannels` channels of `numFrames` samples each.
    // All samples are zeroed. Safe to call again to resize — old data is discarded.
    void allocate(int numChannels, int numFrames);

    // Zero all samples without freeing memory.
    // Call this at the start of each audio block to prevent stale data from leaking
    // into the output when fewer sources are writing than expected.
    void clear();

    // --- Per-channel access ---

    // Returns a writable float* to the start of channel `channel`'s sample array.
    // Returns nullptr if `channel` is out of bounds.
    float* getWritePointer(int channel);

    // Returns a read-only float* to the start of channel `channel`'s sample array.
    // Returns nullptr if `channel` is out of bounds.
    const float* getReadPointer(int channel) const;

    // Returns a float** covering all channels (needed by APIs that want float**).
    // The returned pointer is valid until the next call to allocate() or clear().
    float** getArrayOfWritePointers();

    // Const version of the above (for read-only contexts).
    const float* const* getArrayOfReadPointers() const;

    // --- Info ---

    // Number of audio channels (1 = mono, 2 = stereo, etc.).
    int getNumChannels() const { return numChannels_; }

    // Number of audio frames (samples per channel per block — e.g. 512).
    int getNumFrames() const { return numFrames_; }

    // Total number of floats stored = channels × frames.
    int getNumSamples() const { return numChannels_ * numFrames_; }

private:
    // Flat storage: all channels packed end-to-end.
    // Using a single allocation is cache-friendly and avoids per-channel heap overhead.
    std::vector<float> data_;

    // One raw pointer per channel, each pointing into data_.
    // Pre-computed so getWritePointer(ch) is O(1) and avoids arithmetic in hot loops.
    std::vector<float*> writePointers_;

    int numChannels_ = 0;
    int numFrames_ = 0;
};
