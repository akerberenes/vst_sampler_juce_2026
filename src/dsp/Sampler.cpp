#include "Sampler.h"
#include "effects/Effect.h"
#include <algorithm>
#include <cmath>

Sampler::Sampler()
{
    // All members are initialised via in-class default values — nothing to do here.
}

void Sampler::setSampleData(const float* audioData, int lengthInSamples, int channels)
{
    if (!audioData || lengthInSamples <= 0)
        return;

    // Store all channels interleaved.
    // Total floats = lengthInSamples (frames) × channels.
    // E.g. 1000-frame stereo sample → 2000 floats.
    sampleData_.assign(audioData, audioData + (lengthInSamples * channels));
    channels_ = channels;
    totalFrames_ = lengthInSamples;
}

void Sampler::clearSampleData()
{
    sampleData_.clear();
    totalFrames_ = 0;
    isPlaying_.store(false);
}

void Sampler::setStartFraction(float fraction)
{
    startFraction_ = std::clamp(fraction, 0.0f, 1.0f);
}

void Sampler::setEndFraction(float fraction)
{
    endFraction_ = std::clamp(fraction, 0.0f, 1.0f);
}

void Sampler::setGain(float gain)
{
    // Clamp to [0, 2]: gives a useful range (silence → 6 dB boost) without
    // allowing values that would instantly clip everything downstream.
    gain_.store(std::clamp(gain, 0.0f, 2.0f));
}

void Sampler::trigger(double beatDuration, double tempoInBPM)
{
    if (!hasSampleData())
        return;

    // Convert beats → seconds → samples.
    //   Example: 2 beats at 120 BPM = (2 × 60) / 120 = 1 second = 48000 samples @ 48kHz.
    double durationSeconds = (beatDuration * 60.0) / tempoInBPM;
    int durationSamples = static_cast<int>(durationSeconds * sampleRate_);

    // Reset playhead to the configured start point (in absolute sample index).
    playheadPosition_.store(static_cast<double>(getStartInSamples()));

    // Store the countdown; processBlock() decrements it once per output sample.
    samplesTillStop_.store(durationSamples);

    isPlaying_.store(true);
}

void Sampler::processBlock(float* outAudio, int numSamples, double tempoInBPM)
{
    if (!outAudio || !hasSampleData() || !isPlaying_.load())
    {
        if (outAudio)
            std::fill(outAudio, outAudio + numSamples, 0.0f);
        return;
    }

    // Load atomics into local variables once before the loop.
    // Atomic reads/writes are more expensive than plain variable access;
    // doing them once per block (not once per sample) is much more efficient.
    double pos       = playheadPosition_.load();
    int remaining    = samplesTillStop_.load();
    float gain       = gain_.load();  // Per-pad volume (see setGain).
    int regionStart  = getStartInSamples();
    int regionEnd    = getEndInSamples();

    // Guard: a zero-length region would cause an infinite loop or silence forever.
    if (regionEnd <= regionStart)
        regionEnd = regionStart + 1;

    for (int i = 0; i < numSamples; ++i)
    {
        // Beat-duration countdown: stop when the requested number of samples is done.
        if (remaining <= 0)
        {
            isPlaying_.store(false);
            playheadPosition_.store(pos);
            samplesTillStop_.store(0);
            // Fill the rest of the block with silence.
            std::fill(outAudio + i, outAudio + numSamples, 0.0f);
            return;
        }

        // Write the interpolated sample at the current fractional playhead position,
        // scaled by the per-pad gain (set via setGain(), default 1.0 = unity).
        float sample = interpolateSample(pos) * gain;
        // Apply per-pad effect if one is assigned.
        if (effect_)
            sample = effect_->processSample(sample);
        outAudio[i] = sample;

        // Advance playhead by one sample (normal 1× speed).
        pos += 1.0;

        // Check whether we've hit the end of the active region.
        if (pos >= regionEnd)
        {
            if (loopMode_)
            {
                // Loop: wrap back to the start of the region.
                pos = static_cast<double>(regionStart);
            }
            else
            {
                // One-shot: stop and fill the remaining output with silence.
                isPlaying_.store(false);
                playheadPosition_.store(pos);
                samplesTillStop_.store(0);
                std::fill(outAudio + i + 1, outAudio + numSamples, 0.0f);
                return;
            }
        }

        --remaining;
    }

    // Write locals back to atomics after the loop (one atomic write per block).
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
    int start = getStartInSamples();
    int end   = getEndInSamples();
    int regionLen = end - start;
    if (regionLen <= 0)
        return 0.0f;

    // Express playhead as a fraction of the active region length.
    // 0.0 = at start marker, 1.0 = at end marker.
    return static_cast<float>((playheadPosition_.load() - start) / regionLen);
}

float Sampler::getSampleDurationInSeconds() const
{
    if (totalFrames_ <= 0 || sampleRate_ <= 0)
        return 0.0f;
    return static_cast<float>(totalFrames_) / sampleRate_;
}

int Sampler::getStartInSamples() const
{
    if (totalFrames_ <= 0)
        return 0;
    return static_cast<int>(totalFrames_ * startFraction_);
}

int Sampler::getEndInSamples() const
{
    if (totalFrames_ <= 0)
        return 0;
    return static_cast<int>(totalFrames_ * endFraction_);
}

float Sampler::interpolateSample(double position) const
{
    if (totalFrames_ <= 0)
        return 0.0f;

    int    idx  = static_cast<int>(position);
    float  frac = static_cast<float>(position - idx);

    if (idx < 0 || idx >= totalFrames_)
        return 0.0f;

    float sample1 = sampleData_[idx * channels_];
    float sample2 = (idx + 1 < totalFrames_) ? sampleData_[(idx + 1) * channels_] : 0.0f;

    return sample1 + frac * (sample2 - sample1);
}
