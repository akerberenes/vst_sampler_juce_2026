#pragma once

#include <vector>
#include <atomic>

/**
 * Sampler
 *
 * Plays back a single loaded audio sample (one-shot or looping).
 * One Sampler = one pad on a drum machine.
 *
 * --- Concept ---
 * You load a WAV file into the Sampler once (non-real-time).
 * When a MIDI note arrives, trigger() is called: the playhead resets to
 * startFraction and a sample countdown begins.
 * processBlock() fills the output buffer each audio cycle until playback ends.
 *
 * --- Tempo-synced duration ---
 * Instead of specifying "play for 2 seconds," you say "play for 2 beats."
 * trigger() converts beats → seconds → samples using the current BPM:
 *   durationSamples = (beatDuration * 60 / BPM) * sampleRate
 * This keeps samples aligned to the project grid regardless of tempo changes.
 *
 * --- Playback region ---
 * startFraction / endFraction define the active slice of the loaded audio (0.0–1.0).
 * Example: startFraction=0.5, endFraction=0.75 plays only the third quarter.
 * The UI waveform markers map directly to these fractions.
 *
 * --- Loop mode vs one-shot ---
 * One-shot (default): plays from start to end once, then stops.
 * Loop mode: wraps from endFraction back to startFraction indefinitely
 *            (or until the beat-duration countdown expires).
 *
 * --- Linear interpolation ---
 * The playhead advances by exactly 1.0 sample per output sample.
 * Because position is stored as a double, it can be fractional.
 * interpolateSample() blends two adjacent samples to produce smooth output
 * and avoids the harsh "staircase" distortion of simple truncation.
 *
 * --- Thread safety ---
 * isPlaying_, playheadPosition_, samplesTillStop_ are std::atomic<> so the
 * UI thread can safely poll isPlaying() while the audio thread updates it.
 *
 * No JUCE dependencies — pure C++17.
 */
class Sampler
{
public:
    Sampler();
    ~Sampler() = default;

    // --- Sample data ---

    // Load audio data from a raw interleaved float array.
    // `audioData`         : pointer to float samples (must not be null).
    // `lengthInSamples`   : number of frames (one frame = all channels at one time point).
    // `channels`          : number of interleaved channels in audioData (1=mono, 2=stereo).
    // Only the first channel is used for playback — multi-channel data is stored
    // but only channel 0 is read in interpolateSample().
    void setSampleData(const float* audioData, int lengthInSamples, int channels = 1);

    // Unload the sample and immediately stop any playback.
    void clearSampleData();

    // Returns true when a sample has been loaded via setSampleData().
    bool hasSampleData() const { return !sampleData_.empty(); }

    // --- Configuration ---

    // Set the sample rate in Hz (samples per second).
    // Must match the host's sample rate so beat-to-sample conversion is accurate.
    // Typically 44100 or 48000.
    void setSampleRate(int sr) { sampleRate_ = sr; }
    int getSampleRate() const { return sampleRate_; }

    // Enable loop mode: playhead wraps from endFraction back to startFraction.
    // Disable (default): one-shot — plays once and stops.
    void setLoopMode(bool enable) { loopMode_ = enable; }
    bool getLoopMode() const { return loopMode_; }

    // --- Per-pad output volume ---

    // Set the output gain multiplier applied to every sample this pad produces.
    // 0.0 = silence, 1.0 = unity gain (default), 2.0 = 6 dB boost (~double amplitude).
    // Values are clamped to [0.0, 2.0] to prevent accidental extreme amplification.
    // The gain is applied BEFORE SamplerBank's 0.25 attenuation stage, so the
    // effective range reaching the mix bus is [0.0, 0.5] per pad.
    void setGain(float gain);
    float getGain() const { return gain_.load(); }

    // --- Playback region ---

    // Set the playback start point as a fraction of the full loaded sample (0.0–1.0).
    // Clamped automatically. 0.0 = very beginning, 1.0 = very end.
    void setStartFraction(float fraction);

    // Set the playback end point as a fraction of the full loaded sample (0.0–1.0).
    // Clamped automatically.
    void setEndFraction(float fraction);

    float getStartFraction() const { return startFraction_; }
    float getEndFraction() const { return endFraction_; }

    // --- Playback control ---

    // Start playback.
    // `beatDuration`  : how many beats to play before stopping (e.g. 4.0 = one bar).
    // `tempoInBPM`    : project tempo used to convert beats to samples.
    // Resets the playhead to startFraction and starts the countdown timer.
    void trigger(double beatDuration, double tempoInBPM);

    // Stop playback immediately and zero the countdown.
    void stop();

    // --- Audio generation ---

    // Fill `outAudio` with the next `numSamples` samples of playback audio.
    // Outputs silence (all zeros) when not playing or no sample is loaded.
    // Updates playheadPosition_ and samplesTillStop_ each call.
    void processBlock(float* outAudio, int numSamples, double tempoInBPM);

    // --- State ---

    // Returns true while the sampler is actively generating audio.
    bool isPlaying() const { return isPlaying_.load(); }

    // Returns playhead position as a fraction within the active region:
    // 0.0 = at startFraction, 1.0 = at endFraction.
    float getPlaybackPosition() const;

    // --- Info ---

    // Total number of stored floats (frames × channels).
    int getSampleLengthInSamples() const { return sampleData_.size(); }

    // Duration of the loaded audio in seconds (full sample, ignoring start/end fractions).
    float getSampleDurationInSeconds() const;

private:
    // Raw interleaved sample data: [ch0_f0, ch1_f0, ch0_f1, ch1_f1, ...]
    std::vector<float> sampleData_;

    // Sample rate in Hz. Used to convert beat-duration to sample count.
    int sampleRate_ = 48000;

    // Number of interleaved channels in sampleData_.
    int channels_ = 1;

    // Whether audio is currently playing. Atomic for UI-thread read safety.
    std::atomic<bool> isPlaying_{false};

    // Exact playhead position in samples from the start of sampleData_.
    // Stored as double to accumulate fractional advances without precision loss.
    std::atomic<double> playheadPosition_{0.0};

    // Countdown: how many more output samples to generate before stopping.
    // Set from beat-duration at trigger time; decremented each sample in processBlock.
    std::atomic<int> samplesTillStop_{0};

    // Whether to wrap at endFraction (true) or stop (false).
    bool loopMode_ = false;

    // Active region boundaries as fractions of sampleData_ length.
    float startFraction_ = 0.0f;
    float endFraction_ = 1.0f;

    // Output volume multiplier. Atomic so the UI thread can write it safely
    // while the audio thread reads it inside processBlock().
    std::atomic<float> gain_{1.0f};

    // Convert startFraction_ to an absolute integer sample index in sampleData_.
    int getStartInSamples() const;

    // Convert endFraction_ to an absolute integer sample index in sampleData_.
    int getEndInSamples() const;

    // Compute the output sample at a fractional playhead position using linear interpolation.
    // `position` : exact position in sampleData_ frames (e.g. 1234.7).
    // Returns a weighted blend of the samples at floor(position) and ceil(position).
    float interpolateSample(double position) const;
};
