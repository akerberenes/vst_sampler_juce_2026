#include "CircularBuffer.h"
#include <cmath>
#include <algorithm>

CircularBuffer::CircularBuffer(int maxSizeInSamples)
{
    // Pre-allocate if a size was given at construction time.
    // Passing 0 is valid — allocate() must be called later.
    if (maxSizeInSamples > 0)
        allocate(maxSizeInSamples);
}

void CircularBuffer::allocate(int sizeInSamples)
{
    // Resize the buffer and zero it, then reset all state.
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

    // Only record if not frozen — the tape is locked when the effect is frozen.
    if (!frozen_.load())
    {
        int wp = writePos_.load();
        buffer_[wp] = sample;
        // Advance with wrap-around: % makes the buffer "circular".
        // When wp reaches the end it wraps back to index 0.
        writePos_.store((wp + 1) % buffer_.size());
    }
}

void CircularBuffer::pushBlock(const float* audio, int numSamples)
{
    if (!audio || buffer_.empty())
        return;
    // Push each sample one at a time so the freeze-gate check inside
    // pushSample() applies correctly for every sample in the block.
    for (int i = 0; i < numSamples; ++i)
        pushSample(audio[i]);
}

float CircularBuffer::pullSample()
{
    if (buffer_.empty())
        return 0.0f;

    // Get a loop-wrapped, in-bounds read position.
    double rp = getLoopAdjustedReadPos();

    // Read the sample at that position.
    // Note: rp is fractional but we truncate to int here (nearest-sample read).
    // A higher-quality implementation would interpolate (see Sampler::interpolateSample).
    float sample = buffer_[static_cast<int>(rp)];

    // Advance the read position by playbackSpeed_ samples.
    // speed=0.5 → readPos grows by 0.5 per output sample → half the audio frequency
    //              → sounds one octave lower.
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
    // Clamp to a small positive value to prevent zero (silence) or negative (reverse)
    // playback, neither of which is supported in this implementation.
    playbackSpeed_.store(std::max(0.01f, speed));
}

void CircularBuffer::setReadPosition(float fraction)
{
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    // Convert the 0.0–1.0 fraction to an absolute sample index.
    readPos_.store(fraction * buffer_.size());
}

float CircularBuffer::getReadPosition() const
{
    if (buffer_.empty())
        return 0.0f;
    // Normalise absolute readPos_ back to a 0.0–1.0 fraction of buffer size.
    return static_cast<float>(readPos_.load() / buffer_.size());
}

void CircularBuffer::jumpReadPointerByFraction(float fraction)
{
    if (buffer_.empty())
        return;
    // Jump within the active loop region, not the whole buffer.
    // E.g. fraction=0.25 with a loop region of 48000 samples → jump 12000 samples.
    int loopSize = static_cast<int>(buffer_.size() * (loopEndFraction_ - loopStartFraction_));
    readPos_.store(readPos_.load() + fraction * loopSize);
}

double CircularBuffer::getLoopAdjustedReadPos() const
{
    if (buffer_.empty())
        return 0.0;

    double rp = readPos_.load();

    // Convert fractional boundaries to absolute sample indices.
    int loopStart = static_cast<int>(loopStartFraction_ * buffer_.size());
    int loopEnd   = static_cast<int>(loopEndFraction_   * buffer_.size());
    int loopSize  = loopEnd - loopStart;

    if (loopSize <= 0)
        return 0.0;

    // Wrap rp into [loopStart, loopEnd) using modulo arithmetic.
    // std::fmod gives the remainder of (rp - loopStart) / loopSize,
    // which is the offset within the loop window.
    double relativePos = std::fmod(rp - loopStart, loopSize);

    // std::fmod can return a negative value if (rp - loopStart) is negative
    // (e.g. if readPos_ drifted below loopStart). Add loopSize to correct it.
    if (relativePos < 0.0)
        relativePos += loopSize;

    return loopStart + relativePos;
}
