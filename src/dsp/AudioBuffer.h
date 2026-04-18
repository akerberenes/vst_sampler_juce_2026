#pragma once

#include <vector>
#include <cstring>

/**
 * AudioBuffer
 * 
 * Simple multi-channel audio buffer for inter-module communication.
 * Frame-based (samples × channels).
 * 
 * No JUCE dependencies - pure C++
 */
class AudioBuffer
{
public:
    AudioBuffer() = default;
    explicit AudioBuffer(int numChannels, int numFrames);
    
    // Allocation
    void allocate(int numChannels, int numFrames);
    void clear();
    
    // Access
    float* getWritePointer(int channel);
    const float* getReadPointer(int channel) const;
    float** getArrayOfWritePointers();
    const float* const* getArrayOfReadPointers() const;
    
    // Info
    int getNumChannels() const { return numChannels_; }
    int getNumFrames() const { return numFrames_; }
    int getNumSamples() const { return numChannels_ * numFrames_; }
    
private:
    std::vector<float> data_;
    std::vector<float*> writePointers_;
    int numChannels_ = 0;
    int numFrames_ = 0;
};
