#include "Sampler.h"
#include <algorithm>
#include <cmath>

Sampler::Sampler()
{
}

void Sampler::setSampleData(const float* audioData, int lengthInSamples, int channels)
{
    if (!audioData || lengthInSamples <= 0)
        return;
    
    sampleData_.assign(audioData, audioData + (lengthInSamples * channels));
    channels_ = channels;
}

void Sampler::clearSampleData()
{
    sampleData_.clear();
    isPlaying_.store(false);
}

void Sampler::setLoopFraction(float fraction)
{
    loopFraction_ = std::clamp(fraction, 0.01f, 1.0f);
}

void Sampler::trigger(double beatDuration, double tempoInBPM)
{
    if (!hasSampleData())
        return;
    
    // Calculate duration in samples
    double durationSeconds = (beatDuration * 60.0) / tempoInBPM;
    int durationSamples = static_cast<int>(durationSeconds * sampleRate_);
    
    playheadPosition_.store(0.0);
    samplesTillStop_.store(durationSamples);
    isPlaying_.store(true);
}

void Sampler::processBlock(float* outAudio, int numSamples, double tempoInBPM)
{
    if (!outAudio || !hasSampleData())
    {
        std::fill(outAudio, outAudio + numSamples, 0.0f);
        return;
    }
    
    if (!isPlaying_.load())
    {
        std::fill(outAudio, outAudio + numSamples, 0.0f);
        return;
    }
    
    // Load atomics once before the loop (only audio thread runs this)
    double pos = playheadPosition_.load();
    int remaining = samplesTillStop_.load();
    double loopEnd = getLoopEndInSamples();
    
    for (int i = 0; i < numSamples; ++i)
    {
        if (remaining <= 0)
        {
            isPlaying_.store(false);
            playheadPosition_.store(pos);
            samplesTillStop_.store(0);
            std::fill(outAudio + i, outAudio + numSamples, 0.0f);
            return;
        }
        
        outAudio[i] = interpolateSample(pos);
        
        pos += 1.0;
        if (loopMode_ && pos >= loopEnd)
            pos = 0.0;
        
        --remaining;
    }
    
    // Store atomics once after the loop
    playheadPosition_.store(pos);
    samplesTillStop_.store(remaining);
}

void Sampler::stop()
{
    isPlaying_.store(false);
    samplesTillStop_.store(0);
}

float Sampler::getPlaybackPosition() const
{
    int loopEnd = getLoopEndInSamples();
    if (loopEnd <= 0)
        return 0.0f;
    
    return static_cast<float>(playheadPosition_.load() / loopEnd);
}

float Sampler::getSampleDurationInSeconds() const
{
    if (channels_ <= 0 || sampleRate_ <= 0)
        return 0.0f;
    
    return static_cast<float>(sampleData_.size()) / (channels_ * sampleRate_);
}

int Sampler::getLoopEndInSamples() const
{
    if (channels_ <= 0)
        return 0;
    
    int totalSamples = sampleData_.size() / channels_;
    return static_cast<int>(totalSamples * loopFraction_);
}

float Sampler::interpolateSample(double position) const
{
    if (sampleData_.empty() || channels_ <= 0)
        return 0.0f;
    
    int loopEnd = getLoopEndInSamples();
    if (loopEnd <= 0)
        return 0.0f;
    
    // Simple linear interpolation
    int idx = static_cast<int>(position) % loopEnd;
    double frac = position - static_cast<double>(static_cast<int>(position));
    
    if (idx < 0 || idx >= loopEnd)
        return 0.0f;
    
    float sample1 = sampleData_[idx * channels_];  // First channel
    float sample2 = idx + 1 < loopEnd ? sampleData_[(idx + 1) * channels_] : sampleData_[0];
    
    return sample1 + frac * (sample2 - sample1);
}
