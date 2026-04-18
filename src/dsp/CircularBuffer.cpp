#include "CircularBuffer.h"
#include <cmath>
#include <algorithm>

CircularBuffer::CircularBuffer(int maxSizeInSamples)
{
    if (maxSizeInSamples > 0)
        allocate(maxSizeInSamples);
}

void CircularBuffer::allocate(int sizeInSamples)
{
    buffer_.resize(sizeInSamples, 0.0f);
    writePos_.store(0);
    readPos_.store(0.0);
    clear();
}

void CircularBuffer::clear()
{
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writePos_.store(0);
    readPos_.store(0.0);
    frozen_.store(false);
}

void CircularBuffer::pushSample(float sample)
{
    if (buffer_.empty())
        return;
    
    if (!frozen_.load())
    {
        int wp = writePos_.load();
        buffer_[wp] = sample;
        writePos_.store((wp + 1) % buffer_.size());
    }
}

void CircularBuffer::pushBlock(const float* audio, int numSamples)
{
    if (!audio || buffer_.empty())
        return;
    
    for (int i = 0; i < numSamples; ++i)
        pushSample(audio[i]);
}

float CircularBuffer::pullSample()
{
    if (buffer_.empty())
        return 0.0f;
    
    double rp = getLoopAdjustedReadPos();
    float sample = buffer_[static_cast<int>(rp)];
    
    float speed = playbackSpeed_.load();
    readPos_.store(readPos_.load() + speed);
    
    return sample;
}

void CircularBuffer::pullBlock(float* outAudio, int numSamples)
{
    if (!outAudio || buffer_.empty())
        return;
    
    for (int i = 0; i < numSamples; ++i)
        outAudio[i] = pullSample();
}

void CircularBuffer::freeze()
{
    frozen_.store(true);
}

void CircularBuffer::unfreeze()
{
    frozen_.store(false);
}

bool CircularBuffer::isFrozen() const
{
    return frozen_.load();
}

void CircularBuffer::setLoopStart(float fraction)
{
    loopStartFraction_ = std::clamp(fraction, 0.0f, 1.0f);
}

void CircularBuffer::setLoopEnd(float fraction)
{
    loopEndFraction_ = std::clamp(fraction, 0.0f, 1.0f);
}

void CircularBuffer::setPlaybackSpeed(float speed)
{
    playbackSpeed_.store(std::max(0.01f, speed));
}

void CircularBuffer::setReadPosition(float fraction)
{
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    readPos_.store(fraction * buffer_.size());
}

float CircularBuffer::getReadPosition() const
{
    if (buffer_.empty())
        return 0.0f;
    return static_cast<float>(readPos_.load() / buffer_.size());
}

void CircularBuffer::jumpReadPointerByFraction(float fraction)
{
    if (buffer_.empty())
        return;
    
    int loopSize = static_cast<int>(buffer_.size() * (loopEndFraction_ - loopStartFraction_));
    readPos_.store(readPos_.load() + fraction * loopSize);
}

double CircularBuffer::getLoopAdjustedReadPos() const
{
    if (buffer_.empty())
        return 0.0;
    
    double rp = readPos_.load();
    int loopStart = static_cast<int>(loopStartFraction_ * buffer_.size());
    int loopEnd = static_cast<int>(loopEndFraction_ * buffer_.size());
    int loopSize = loopEnd - loopStart;
    
    if (loopSize <= 0)
        return 0.0;
    
    // Wrap read position within loop bounds
    double relativePos = std::fmod(rp - loopStart, loopSize);
    if (relativePos < 0.0)
        relativePos += loopSize;
    
    return loopStart + relativePos;
}
