#include "AudioFileLoader.h"

AudioFileLoader::LoadResult AudioFileLoader::loadAudioFile(const std::string& filePath)
{
    // --- Stub ---
    // This function will be implemented for the Teensy 4.1 port.
    // A full implementation would:
    //   1. Open `filePath` and read the first bytes to detect the format.
    //      (WAV files start with "RIFF"; FLAC starts with "fLaC"; MP3 with 0xFF 0xFB)
    //   2. Dispatch to the appropriate decoder (decodeWAV, decodeFLAC, etc.).
    //   3. Return decoded float samples in result.audioData, interleaved by channel.
    //
    // For the VST3/Standalone build, PluginProcessor uses JUCE's AudioFormatManager
    // instead, so this stub is never called in that context.
    LoadResult result;
    result.success      = false;
    result.errorMessage = "Audio file loading not yet implemented";
    return result;
}
