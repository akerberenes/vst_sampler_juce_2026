#pragma once

#include <vector>
#include <string>

/**
 * AudioFileLoader
 *
 * Thin wrapper for loading audio files from disk into CPU memory.
 *
 * --- Purpose ---
 * PluginProcessor uses JUCE's AudioFormatManager directly, so for the
 * VST3/standalone build this class is currently unused. It exists as a
 * foundation for the Teensy 4.1 port, where a JUCE-independent loader
 * will be needed (reading WAV headers via SDLib, for example).
 *
 * --- LoadResult ---
 * The result is returned by value rather than via output parameters or
 * exceptions. This is idiomatic modern C++: the caller can inspect `success`
 * and branch, and the audio data is owned by the struct.
 *
 * --- Planned formats ---
 * WAV  -- simplest to implement (raw PCM with a short 44-byte header).
 * MP3  -- needs a decoder library (e.g. dr_mp3).
 * FLAC -- lossless; needs libFLAC or similar.
 */
class AudioFileLoader
{
public:
    // Result object returned from every load operation.
    struct LoadResult
    {
        bool success = false;          // true if the file was read without errors.
        std::vector<float> audioData;  // Decoded samples, interleaved by channel.
        int sampleRate = 0;            // The native sample rate of the audio file.
        int channels   = 0;            // Number of interleaved audio channels.
        std::string errorMessage;      // Human-readable error; empty on success.
    };

    // Load any supported audio file by path.
    // Returns a LoadResult; check `success` before using `audioData`.
    // Currently a stub -- see AudioFileLoader.cpp.
    static LoadResult loadAudioFile(const std::string& filePath);

private:
    // Per-format decoder helpers (all TODO):
    // static LoadResult decodeWAV(const std::string& filePath);
    // static LoadResult decodeMP3(const std::string& filePath);
    // static LoadResult decodeFLAC(const std::string& filePath);
};
