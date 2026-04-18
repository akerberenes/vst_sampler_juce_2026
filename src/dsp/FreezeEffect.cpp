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

    float wetMix = dryWetMix_.load();

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
        float frozenAudio[4096];
        freezeBuffer_.pullBlock(frozenAudio, numSamples);

        // Crossfade between the original (dry) input and the frozen (wet) audio.
        // wetMix=0.0 → only dry;  wetMix=1.0 → only frozen.
        if (inputAudio)
        {
            for (int i = 0; i < numSamples; ++i)
                outputAudio[i] = inputAudio[i] * (1.0f - wetMix) + frozenAudio[i] * wetMix;
        }
        else
        {
            // No dry signal: output is entirely the frozen audio.
            std::memcpy(outputAudio, frozenAudio, numSamples * sizeof(float));
        }
    }
}

void FreezeEffect::setDryWetMix(float wet)
{
    dryWetMix_.store(std::clamp(wet, 0.0f, 1.0f));
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
    // Example at 120 BPM, stutterFraction=0.125 (1/8 beat):
    //   samplesPerBeat  = (60 / 120) * 48000 = 24000 samples
    //   stutterSamples  = 24000 * 0.125      = 3000 samples  (~62.5 ms)
    double samplesPerBeat = (60.0 / tempoInBPM) * sampleRate_;
    double stutterSamples = samplesPerBeat * stutterFraction_;

    // Accumulate the block size. When enough samples have passed to complete
    // one stutter interval, jump the read pointer and reset the accumulator.
    stutterAccumulator_ += blockSize;
    if (stutterAccumulator_ >= stutterSamples)
    {
        // Jump the read pointer forward by stutterFraction of the loop region.
        // This restarts the loop pattern at the current position + a small offset,
        // creating the characteristic rhythmic repeat sound.
        freezeBuffer_.jumpReadPointerByFraction(stutterFraction_);
        stutterAccumulator_ -= stutterSamples;  // Carry over remainder (stays in sync).
    }
}
