#include "AudioBuffer.h"

AudioBuffer::AudioBuffer(int numChannels, int numFrames)
{
    allocate(numChannels, numFrames);
}

void AudioBuffer::allocate(int numChannels, int numFrames)
{
    numChannels_ = numChannels;
    numFrames_ = numFrames;
    
    data_.resize(numChannels * numFrames, 0.0f);
    
    // Set up channel pointers into the flat data array
    writePointers_.clear();
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        writePointers_.push_back(data_.data() + ch * numFrames);
    }
}

void AudioBuffer::clear()
{
    std::fill(data_.begin(), data_.end(), 0.0f);
}

float* AudioBuffer::getWritePointer(int channel)
{
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

float** AudioBuffer::getArrayOfWritePointers()
{
    return writePointers_.data();
}

const float* const* AudioBuffer::getArrayOfReadPointers() const
{
    return const_cast<const float* const*>(writePointers_.data());
}
