#include "FreezeEffect.h"
#include <cmath>
#include <algorithm>

FreezeEffect::FreezeEffect(int maxBufferSizeInSamples)
    : freezeBuffer_(maxBufferSizeInSamples)
{
    // If maxBufferSizeInSamples > 0, CircularBuffer is already allocated.
    // Otherwise, prepare() will allocate it when the host calls prepareToPlay().
}

void FreezeEffect::prepare(int sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;

    // Allocate a 4-second ring buffer (if not already allocated).
    // 4 seconds covers about 2 bars at 120 BPM, which is a good default freeze window.
    // Calculation: 4 seconds × sampleRate samples/second = sampleRate × 4 samples.
    int defaultSize = sampleRate * 4;
    if (freezeBuffer_.getSize() == 0)
        freezeBuffer_.allocate(defaultSize);
}

void FreezeEffect::setBufferSizeInSamples(int size)
{
    // Reallocate the ring buffer to a custom size.
    // This resets all content and pointers (clear() is called inside allocate()).
    freezeBuffer_.allocate(size);
}

void FreezeEffect::setFrozen(bool b)
{
    // Delegate freeze/unfreeze to the CircularBuffer.
    // Freezing stops the write pointer; unfreezing resumes it.
    if (b)
        freezeBuffer_.freeze();
    else
        freezeBuffer_.unfreeze();
}

void FreezeEffect::processBlock(const float* inputAudio, float* outputAudio, int numSamples,
                                double tempoInBPM)
{
    if (!outputAudio || numSamples <= 0 || numSamples > 4096)
        return;

    if (!isFrozen())
    {
        // --- Recording state ---
        // Push the incoming audio into the ring buffer so it's ready when frozen.
        freezeBuffer_.pushBlock(inputAudio, numSamples);

        // Output the dry signal unchanged (pass-through).
        if (inputAudio)
            std::memcpy(outputAudio, inputAudio, numSamples * sizeof(float));
        else
            std::fill(outputAudio, outputAudio + numSamples, 0.0f);
    }
    else
    {
        // --- Frozen state ---
        // First: check if a stutter jump is due this block.
        updateStutterPlayback(tempoInBPM, numSamples);

        // Pull the looping frozen audio from the ring buffer.
        // Output is always 100% wet — the frozen loop only, no dry signal blended in.
        freezeBuffer_.pullBlock(outputAudio, numSamples);
    }
}

void FreezeEffect::setLoopLength(float length)
{
    loopLength_ = std::clamp(length, 0.0f, 1.0f);
    applyLoopBounds();
}

void FreezeEffect::setLoopPosition(float position)
{
    loopPosition_ = std::clamp(position, 0.0f, 1.0f);
    applyLoopBounds();
}

void FreezeEffect::applyLoopBounds()
{
    // Clamp start so the window never exceeds the buffer end.
    float start = std::clamp(loopPosition_, 0.0f, 1.0f - loopLength_);
    float end   = start + loopLength_;
    freezeBuffer_.setLoopStart(start);
    freezeBuffer_.setLoopEnd(end);
}

void FreezeEffect::copyBufferSnapshot(std::vector<float>& out) const
{
    int size = freezeBuffer_.getSize();
    out.resize(size);
    if (size > 0)
        std::memcpy(out.data(), freezeBuffer_.getBuffer(), size * sizeof(float));
}

float FreezeEffect::getBufferFillPercentage() const
{
    if (freezeBuffer_.getSize() <= 0)
        return 0.0f;
    // Read position as a 0–100% fill indicator.
    return freezeBuffer_.getReadPosition() * 100.0f;
}

void FreezeEffect::updateStutterPlayback(double tempoInBPM, int blockSize)
{
    // Calculate how many samples correspond to one stutter interval.
    // stutterFraction is a fraction of one beat; samplesPerBeat = 60/BPM * sampleRate.
    // Example at 120 BPM, stutterFraction=0.015625 (1/64 beat):
    //   samplesPerBeat  = (60 / 120) * 48000 = 24000 samples
    //   stutterSamples  = 24000 * 0.015625   = 375 samples  (~7.8 ms)
    //   → read pointer resets 64 times per beat = very fast choppy retrigger.
    double samplesPerBeat = (60.0 / tempoInBPM) * sampleRate_;
    double stutterSamples = samplesPerBeat * stutterFraction_;

    // Accumulate the block size. When enough samples have passed to complete
    // one stutter interval, reset the read pointer to the loop start.
    stutterAccumulator_ += blockSize;
    if (stutterAccumulator_ >= stutterSamples)
    {
        // Reset read pointer to the beginning of the loop window.
        // This is the "retrigger" — the frozen audio restarts from the loop
        // start position every stutterFraction of a beat.
        // With speed > 1.0 the audio plays faster within each grain, so the
        // tail of each stutter interval may run out of content earlier,
        // producing a shorter, higher-pitched "click" that still retriggers
        // at the rhythmic stutter rate.
        float start = std::clamp(loopPosition_, 0.0f, 1.0f - loopLength_);
        freezeBuffer_.setReadPosition(start);
        stutterAccumulator_ -= stutterSamples;  // Carry over remainder (stays in sync).
    }
}
