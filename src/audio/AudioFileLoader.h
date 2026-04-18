#pragma once

#include <vector>
#include <string>

/**
 * AudioFileLoader
 * 
 * Loads audio files (WAV, MP3, FLAC) into float arrays.
 * Non-real-time operation.
 */
class AudioFileLoader
{
public:
    struct LoadResult
    {
        bool success = false;
        std::vector<float> audioData;
        int sampleRate = 0;
        int channels = 0;
        std::string errorMessage;
    };
    
    static LoadResult loadAudioFile(const std::string& filePath);
    
private:
    // Implementations for each format (TODO)
};
