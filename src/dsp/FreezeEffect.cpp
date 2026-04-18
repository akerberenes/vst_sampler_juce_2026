#include "FreezeEffect.h"
#include <cmath>
#include <algorithm>

FreezeEffect::FreezeEffect(int maxBufferSizeInSamples)
{
    freezeBuffer_ = std::make_unique<CircularBuffer>(maxBufferSizeInSamples);
}

void FreezeEffect::prepare(int sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;
    
    // Default 2 bars at 120 BPM, 48kHz
    // 2 bars = 8 beats at 120 BPM = 8 * (60/120) seconds = 4 seconds = 192000 samples
    int defaultSize = sampleRate * 4;  // 4 seconds
    if (freezeBuffer_->getSize() == 0)
        freezeBuffer_->allocate(defaultSize);
    
    freezeBuffer_->setSampleRate(sampleRate);
}

void FreezeEffect::setBufferSizeInSamples(int size)
{
    freezeBuffer_->allocate(size);
}

void FreezeEffect::setFrozen(bool b)
{
    if (b)
        freezeBuffer_->freeze();
    else
        freezeBuffer_->unfreeze();
}

void FreezeEffect::processBlock(const float* inputAudio, float* outputAudio, int numSamples,
                                double tempoInBPM)
{
    if (!outputAudio || numSamples <= 0)
        return;
    
    float wetMix = dryWetMix_.load();
    
    if (!isFrozen())
    {
        // Recording mode: push input to buffer and output dry signal
        freezeBuffer_->pushBlock(inputAudio, numSamples);
        if (inputAudio)
            std::memcpy(outputAudio, inputAudio, numSamples * sizeof(float));
        else
            std::fill(outputAudio, outputAudio + numSamples, 0.0f);
    }
    else
    {
        // Frozen mode: pull from buffer with stutter/speed applied
        updateStutterPlayback(tempoInBPM, numSamples);
        
        float frozenAudio[4096];
        freezeBuffer_->pullBlock(frozenAudio, numSamples);
        
        // Crossfade between dry and frozen
        if (inputAudio)
        {
            for (int i = 0; i < numSamples; ++i)
                outputAudio[i] = inputAudio[i] * (1.0f - wetMix) + frozenAudio[i] * wetMix;
        }
        else
        {
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
    if (freezeBuffer_->getSize() <= 0)
        return 0.0f;
    
    return freezeBuffer_->getReadPosition() * 100.0f;
}

void FreezeEffect::updateStutterPlayback(double tempoInBPM, int blockSize)
{
    // Calculate samples per beat
    double samplesPerBeat = (60.0 / tempoInBPM) * sampleRate_;
    double stutterSamples = samplesPerBeat * stutterFraction_;
    
    // Accumulate and jump every stutter interval
    stutterAccumulator_ += blockSize;
    if (stutterAccumulator_ >= stutterSamples)
    {
        freezeBuffer_->jumpReadPointerByFraction(stutterFraction_);
        stutterAccumulator_ = 0.0;
    }
}
