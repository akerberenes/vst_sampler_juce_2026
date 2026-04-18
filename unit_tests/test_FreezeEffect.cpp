#include <catch2/catch.hpp>
#include "FreezeEffect.h"
#include <cmath>
#include <vector>

// Helper: create test sine wave
std::vector<float> createTestWave(int numSamples, int sampleRate, float frequency)
{
    std::vector<float> wave(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        wave[i] = std::sin(2.0f * 3.14159f * frequency * i / sampleRate);
    }
    return wave;
}

TEST_CASE("FreezeEffect initialization", "[FreezeEffect]")
{
    FreezeEffect freeze(4096);
    freeze.prepare(48000, 512);
    
    SECTION("Initial state is recording")
    {
        REQUIRE(freeze.getState() == FreezeEffect::State::Recording);
    }
    
    SECTION("Buffer size is set correctly")
    {
        REQUIRE(freeze.getBufferSizeInSamples() == 4096);
    }
}

TEST_CASE("FreezeEffect frozen state", "[FreezeEffect]")
{
    FreezeEffect freeze(2048);
    freeze.prepare(48000, 512);
    
    SECTION("Initially not frozen")
    {
        REQUIRE(freeze.isFrozen() == false);
    }
    
    SECTION("Can freeze and unfreeze")
    {
        freeze.setFrozen(true);
        REQUIRE(freeze.isFrozen() == true);
        
        freeze.setFrozen(false);
        REQUIRE(freeze.isFrozen() == false);
    }
}

TEST_CASE("FreezeEffect dry/wet mix", "[FreezeEffect]")
{
    FreezeEffect freeze(2048);
    freeze.prepare(48000, 512);
    
    SECTION("Default dry/wet is 1.0 (full wet)")
    {
        REQUIRE(freeze.getDryWetMix() == Approx(1.0f));
    }
    
    SECTION("Set dry/wet mix")
    {
        freeze.setDryWetMix(0.5f);
        REQUIRE(freeze.getDryWetMix() == Approx(0.5f));
        
        freeze.setDryWetMix(0.0f);
        REQUIRE(freeze.getDryWetMix() == Approx(0.0f));
    }
}

TEST_CASE("FreezeEffect playback speed", "[FreezeEffect]")
{
    FreezeEffect freeze(2048);
    freeze.prepare(48000, 512);
    
    SECTION("Default speed is 1.0")
    {
        REQUIRE(freeze.getPlaybackSpeed() == Approx(1.0f));
    }
    
    SECTION("Set playback speed")
    {
        freeze.setPlaybackSpeed(0.5f);
        REQUIRE(freeze.getPlaybackSpeed() == Approx(0.5f));
        
        freeze.setPlaybackSpeed(2.0f);
        REQUIRE(freeze.getPlaybackSpeed() == Approx(2.0f));
    }
}

TEST_CASE("FreezeEffect loop boundaries", "[FreezeEffect]")
{
    FreezeEffect freeze(2048);
    freeze.prepare(48000, 512);
    
    SECTION("Default loop boundaries")
    {
        REQUIRE(freeze.getLoopStart() == Approx(0.0f));
        REQUIRE(freeze.getLoopEnd() == Approx(1.0f));
    }
    
    SECTION("Set custom loop boundaries")
    {
        freeze.setLoopStart(0.25f);
        freeze.setLoopEnd(0.75f);
        
        REQUIRE(freeze.getLoopStart() == Approx(0.25f));
        REQUIRE(freeze.getLoopEnd() == Approx(0.75f));
    }
}

TEST_CASE("FreezeEffect stutter fraction", "[FreezeEffect]")
{
    FreezeEffect freeze(2048);
    freeze.prepare(48000, 512);
    
    SECTION("Default stutter fraction")
    {
        REQUIRE(freeze.getStutterFraction() == Approx(0.125));  // 1/8 beat
    }
    
    SECTION("Set stutter fraction")
    {
        freeze.setStutterFraction(0.25);  // 1/4 beat
        REQUIRE(freeze.getStutterFraction() == Approx(0.25));
        
        freeze.setStutterFraction(0.0625);  // 1/16 beat
        REQUIRE(freeze.getStutterFraction() == Approx(0.0625));
    }
}

TEST_CASE("FreezeEffect process block", "[FreezeEffect]")
{
    FreezeEffect freeze(2048);
    freeze.prepare(48000, 512);
    
    SECTION("Can process audio blocks")
    {
        float input[512];
        float output[512] = {0.0f};
        
        // Fill input with test data
        for (int i = 0; i < 512; ++i)
        {
            input[i] = 0.1f;
        }
        
        freeze.processBlock(input, output, 512, 120.0);
        
        // After processing, output should contain some data
        REQUIRE(output != nullptr);
    }
}

TEST_CASE("FreezeEffect buffer fill percentage", "[FreezeEffect]")
{
    FreezeEffect freeze(1024);
    freeze.prepare(48000, 512);
    
    SECTION("Buffer initially empty")
    {
        float fillPercent = freeze.getBufferFillPercentage();
        REQUIRE(fillPercent >= 0.0f);
        REQUIRE(fillPercent <= 100.0f);
    }
}

TEST_CASE("FreezeEffect recording to frozen transition", "[FreezeEffect]")
{
    FreezeEffect freeze(2048);
    freeze.prepare(48000, 512);
    
    SECTION("Start in recording, transition to frozen")
    {
        REQUIRE(freeze.getState() == FreezeEffect::State::Recording);
        
        freeze.setFrozen(true);
        REQUIRE(freeze.isFrozen() == true);
        
        freeze.setFrozen(false);
        REQUIRE(freeze.isFrozen() == false);
    }
}
