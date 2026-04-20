#include <catch2/catch.hpp>
#include "CircularBuffer.h"
#include <cmath>

using namespace Catch;

TEST_CASE("CircularBuffer allocation", "[CircularBuffer]")
{
    SECTION("Constructor with size")
    {
        CircularBuffer cb(4096);
        REQUIRE(cb.getSize() == 4096);
    }
    
    SECTION("Allocate method")
    {
        CircularBuffer cb;
        cb.allocate(2048);
        REQUIRE(cb.getSize() == 2048);
    }
}

TEST_CASE("CircularBuffer push/pull samples", "[CircularBuffer]")
{
    CircularBuffer cb(512);
    
    SECTION("Push single sample")
    {
        cb.pushSample(0.5f);
        cb.setReadPosition(0.0f);
        float result = cb.pullSample();
        REQUIRE(result == Approx(0.5f));
    }
    
    SECTION("Push block of samples")
    {
        float testData[4] = {0.1f, 0.2f, 0.3f, 0.4f};
        cb.pushBlock(testData, 4);
        
        cb.setReadPosition(0.0f);
        float out[4];
        cb.pullBlock(out, 4);
        
        REQUIRE(out[0] == Approx(0.1f));
        REQUIRE(out[1] == Approx(0.2f));
        REQUIRE(out[2] == Approx(0.3f));
        REQUIRE(out[3] == Approx(0.4f));
    }
}

TEST_CASE("CircularBuffer freeze state", "[CircularBuffer]")
{
    CircularBuffer cb(256);
    
    SECTION("Initially not frozen")
    {
        REQUIRE(cb.isFrozen() == false);
    }
    
    SECTION("Freeze and unfreeze")
    {
        cb.freeze();
        REQUIRE(cb.isFrozen() == true);
        
        cb.unfreeze();
        REQUIRE(cb.isFrozen() == false);
    }
}

TEST_CASE("CircularBuffer loop boundaries", "[CircularBuffer]")
{
    CircularBuffer cb(1024);
    
    SECTION("Default loop boundaries")
    {
        REQUIRE(cb.getLoopStart() == Approx(0.0f));
        REQUIRE(cb.getLoopEnd() == Approx(1.0f));
    }
    
    SECTION("Set custom loop boundaries")
    {
        cb.setLoopStart(0.25f);
        cb.setLoopEnd(0.75f);
        
        REQUIRE(cb.getLoopStart() == Approx(0.25f));
        REQUIRE(cb.getLoopEnd() == Approx(0.75f));
    }
}

TEST_CASE("CircularBuffer playback speed", "[CircularBuffer]")
{
    CircularBuffer cb(512);
    
    SECTION("Default speed is 1.0")
    {
        REQUIRE(cb.getPlaybackSpeed() == Approx(1.0f));
    }
    
    SECTION("Set and verify speed")
    {
        cb.setPlaybackSpeed(0.5f);
        REQUIRE(cb.getPlaybackSpeed() == Approx(0.5f));
        
        cb.setPlaybackSpeed(2.0f);
        REQUIRE(cb.getPlaybackSpeed() == Approx(2.0f));
    }
}

TEST_CASE("CircularBuffer read position", "[CircularBuffer]")
{
    CircularBuffer cb(256);
    
    SECTION("Set read position")
    {
        cb.setReadPosition(0.5f);
        float pos = cb.getReadPosition();
        REQUIRE(pos == Approx(0.5f).epsilon(0.01f));
    }
    
}

TEST_CASE("CircularBuffer clear", "[CircularBuffer]")
{
    CircularBuffer cb(128);
    
    // Fill with data
    for (int i = 0; i < 128; ++i)
    {
        cb.pushSample(0.5f);
    }
    
    cb.clear();
    
    // After clear, buffer should be empty
    cb.setReadPosition(0.0f);
    float sample = cb.pullSample();
    REQUIRE(sample == Approx(0.0f));
}
