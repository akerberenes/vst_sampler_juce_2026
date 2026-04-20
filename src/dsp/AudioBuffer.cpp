#include "AudioBuffer.h"

AudioBuffer::AudioBuffer(int numChannels, int numFrames)
{
    // Delegate to allocate() so all initialisation logic lives in one place.
    allocate(numChannels, numFrames);
}

void AudioBuffer::allocate(int numChannels, int numFrames)
{
    numChannels_ = numChannels;
    numFrames_ = numFrames;

    // Allocate flat storage: channels × frames floats, initialised to 0.
    // Layout: [ ch0_f0, ch0_f1, ..., ch0_fN, ch1_f0, ch1_f1, ..., ch1_fN ]
    data_.resize(numChannels * numFrames, 0.0f);

    // Rebuild the per-channel pointer table.
    // Each channel pointer points `ch * numFrames` elements into data_.
    // This means channel 0 starts at index 0, channel 1 at index numFrames, etc.
    writePointers_.clear();
    for (int ch = 0; ch < numChannels; ++ch)
        writePointers_.push_back(data_.data() + ch * numFrames);
}

void AudioBuffer::clear()
{
    // std::fill is a no-op on an empty range — safe to call even before allocate().
    std::fill(data_.begin(), data_.end(), 0.0f);
}

float* AudioBuffer::getWritePointer(int channel)
{
    // Bounds check prevents out-of-range access that would corrupt memory.
    if (channel < 0 || channel >= numChannels_ || writePointers_.empty())
        return nullptr;
    return writePointers_[channel];
}

const float* AudioBuffer::getReadPointer(int channel) const
{
    if (channel < 0 || channel >= numChannels_ || writePointers_.empty())
        return nullptr;
    return writePointers_[channel];
}
