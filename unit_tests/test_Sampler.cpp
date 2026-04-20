#include <catch2/catch.hpp>
#include "Sampler.h"
#include <cmath>

using namespace Catch;

// Helper: create a test sine wave
inline std::vector<float> createTestWave(int numSamples, int sampleRate, float frequency)
{
    std::vector<float> wave(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        wave[i] = std::sin(2.0f * 3.14159f * frequency * i / sampleRate);
    }
    return wave;
}

TEST_CASE("Sampler initialization", "[Sampler]")
{
    Sampler sampler;
    sampler.setSampleRate(48000);
    
    SECTION("Default state")
    {
        REQUIRE(sampler.hasSampleData() == false);
        REQUIRE(sampler.isPlaying() == false);
        REQUIRE(sampler.getSampleRate() == 48000);
    }
    
    SECTION("Sample rate")
    {
        sampler.setSampleRate(44100);
        REQUIRE(sampler.getSampleRate() == 44100);
    }
}

TEST_CASE("Sampler sample data", "[Sampler]")
{
    Sampler sampler;
    sampler.setSampleRate(48000);
    
    SECTION("Load sample data")
    {
        auto testWave = createTestWave(1024, 48000, 440.0f);
        sampler.setSampleData(testWave.data(), testWave.size());
        
        REQUIRE(sampler.hasSampleData() == true);
        REQUIRE(sampler.getSampleLengthInSamples() == 1024);
    }
    
    SECTION("Clear sample data")
    {
        auto testWave = createTestWave(512, 48000, 440.0f);
        sampler.setSampleData(testWave.data(), testWave.size());
        REQUIRE(sampler.hasSampleData() == true);
        
        sampler.clearSampleData();
        REQUIRE(sampler.hasSampleData() == false);
    }
}

TEST_CASE("Sampler loop mode", "[Sampler]")
{
    Sampler sampler;
    
    SECTION("Default loop mode is off")
    {
        REQUIRE(sampler.getLoopMode() == false);
    }
    
    SECTION("Set loop mode")
    {
        sampler.setLoopMode(true);
        REQUIRE(sampler.getLoopMode() == true);
        
        sampler.setLoopMode(false);
        REQUIRE(sampler.getLoopMode() == false);
    }
}

TEST_CASE("Sampler trigger", "[Sampler]")
{
    Sampler sampler;
    sampler.setSampleRate(48000);
    auto testWave = createTestWave(48000, 48000, 440.0f);
    sampler.setSampleData(testWave.data(), testWave.size());
    
    SECTION("Trigger starts playback")
    {
        sampler.trigger(1.0, 120.0);  // 1 beat at 120 BPM
        REQUIRE(sampler.isPlaying() == true);
    }
    
    SECTION("Stop ends playback")
    {
        sampler.trigger(1.0, 120.0);
        REQUIRE(sampler.isPlaying() == true);
        
        sampler.stop();
        REQUIRE(sampler.isPlaying() == false);
    }
}

TEST_CASE("Sampler tempo sync calculation", "[Sampler]")
{
    Sampler sampler;
    sampler.setSampleRate(48000);
    auto testWave = createTestWave(96000, 48000, 440.0f);
    sampler.setSampleData(testWave.data(), testWave.size());
    
    SECTION("4 beats at 120 BPM = 2 seconds = 96,000 samples @ 48kHz")
    {
        sampler.trigger(4.0, 120.0);
        // Duration = (4 beats / 120 BPM) * 60 sec/min = 2 seconds
        // = 2 * 48000 = 96,000 samples
        REQUIRE(sampler.isPlaying() == true);
    }
    
    SECTION("1 beat at 60 BPM = 1 second = 48,000 samples @ 48kHz")
    {
        sampler.trigger(1.0, 60.0);
        REQUIRE(sampler.isPlaying() == true);
    }
}

TEST_CASE("Sampler playback position", "[Sampler]")
{
    Sampler sampler;
    sampler.setSampleRate(48000);
    auto testWave = createTestWave(1024, 48000, 440.0f);
    sampler.setSampleData(testWave.data(), testWave.size());
    
    SECTION("Initial playback position is 0")
    {
        REQUIRE(sampler.getPlaybackPosition() == Approx(0.0f));
    }
}

TEST_CASE("Sampler sample duration", "[Sampler]")
{
    Sampler sampler;
    sampler.setSampleRate(48000);
    
    // 1 second of audio
    auto testWave = createTestWave(48000, 48000, 440.0f);
    sampler.setSampleData(testWave.data(), testWave.size());
    
    float duration = sampler.getSampleDurationInSeconds();
    REQUIRE(duration == Approx(1.0f).epsilon(0.01f));
}
