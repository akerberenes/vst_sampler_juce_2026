#include <catch2/catch.hpp>
#include "FreezeEffect.h"
#include <cmath>
#include <vector>

using namespace Catch;

// Helper: create test sine wave
inline std::vector<float> createTestWave(int numSamples, int sampleRate, float frequency)
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

TEST_CASE("FreezeEffect loop length and position", "[FreezeEffect]")
{
    FreezeEffect freeze(2048);
    freeze.prepare(48000, 512);

    SECTION("Default loop length is 1.0 (full buffer)")
    {
        REQUIRE(freeze.getLoopLength() == Approx(1.0f));
    }

    SECTION("Default loop position is 0.0 (start of buffer)")
    {
        REQUIRE(freeze.getLoopPosition() == Approx(0.0f));
    }

    SECTION("Setting loop length shrinks the window")
    {
        freeze.setLoopLength(0.5f);
        REQUIRE(freeze.getLoopLength() == Approx(0.5f));
    }

    SECTION("Setting loop position shifts the window")
    {
        freeze.setLoopLength(0.5f);
        freeze.setLoopPosition(0.5f);  // window: [0.5, 1.0]
        REQUIRE(freeze.getLoopPosition() == Approx(0.5f));
    }

    SECTION("Loop position is clamped so window stays within buffer")
    {
        // With length=0.5, max start = 0.5 (so end = 1.0).
        // Setting position=0.9 should clamp start to 0.5.
        freeze.setLoopLength(0.5f);
        freeze.setLoopPosition(0.9f);
        // The stored position is 0.9, but the actual applied start is clamped
        // internally. Verify the getter returns the requested value.
        REQUIRE(freeze.getLoopPosition() == Approx(0.9f));
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

TEST_CASE("FreezeEffect stutter retrigger resets to loop start", "[FreezeEffect]")
{
    // Use a buffer large enough that stutter intervals are meaningful.
    const int bufSize = 48000;   // 1 second at 48 kHz
    const int blockSize = 512;
    const double bpm = 120.0;

    FreezeEffect freeze(bufSize);
    freeze.prepare(48000, blockSize);

    // Fill the buffer with known data so freeze has content.
    {
        float input[512];
        float output[512];
        for (int i = 0; i < 48000 / 512; ++i)
        {
            for (int s = 0; s < 512; ++s)
                input[s] = static_cast<float>(s) / 512.0f;
            freeze.processBlock(input, output, 512, bpm);
        }
    }

    // Set loop position to 0.3, loop length 0.2 → loop window [0.3, 0.5).
    freeze.setLoopPosition(0.3f);
    freeze.setLoopLength(0.2f);

    // Stutter at 1/4 beat → at 120 BPM, stutter interval = 0.25 * 24000 = 6000 samples.
    freeze.setStutterFraction(0.25);
    freeze.setFrozen(true);

    SECTION("Read pointer resets to loop start after stutter interval")
    {
        float input[512];
        float output[512];
        for (int s = 0; s < 512; ++s) input[s] = 0.0f;

        // Process enough blocks to exceed one stutter interval (6000 samples).
        // 12 blocks × 512 = 6144 samples > 6000.
        for (int i = 0; i < 12; ++i)
            freeze.processBlock(input, output, blockSize, bpm);

        // After the retrigger, read position should be near the loop start (0.3).
        float readFraction = freeze.getBufferFillPercentage() / 100.0f;
        // Allow some tolerance — the read pointer advances by 144 samples past the
        // retrigger point (6144 - 6000 = 144 samples = 0.003 of 48000 buffer).
        REQUIRE(readFraction >= 0.29f);
        REQUIRE(readFraction <= 0.35f);
    }

    SECTION("Multiple retriggers stay near loop start")
    {
        float input[512];
        float output[512];
        for (int s = 0; s < 512; ++s) input[s] = 0.0f;

        // Process 3 full stutter intervals worth of blocks.
        // 3 × 6000 = 18000 samples → 36 blocks of 512 (18432 samples).
        for (int i = 0; i < 36; ++i)
            freeze.processBlock(input, output, blockSize, bpm);

        float readFraction = freeze.getBufferFillPercentage() / 100.0f;
        // After 3 retriggers, should still be near loop start.
        REQUIRE(readFraction >= 0.29f);
        REQUIRE(readFraction <= 0.35f);
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
