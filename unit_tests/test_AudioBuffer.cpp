#include <catch2/catch.hpp>
#include "AudioBuffer.h"

using namespace Catch;

TEST_CASE("AudioBuffer allocation", "[AudioBuffer]")
{
    SECTION("Default constructor")
    {
        AudioBuffer buf;
        REQUIRE(buf.getNumChannels() == 0);
        REQUIRE(buf.getNumFrames() == 0);
        REQUIRE(buf.getNumSamples() == 0);
    }
    
    SECTION("Constructor with dimensions")
    {
        AudioBuffer buf(2, 1024);
        REQUIRE(buf.getNumChannels() == 2);
        REQUIRE(buf.getNumFrames() == 1024);
        REQUIRE(buf.getNumSamples() == 2048);
    }
    
    SECTION("Allocate method")
    {
        AudioBuffer buf;
        buf.allocate(4, 512);
        REQUIRE(buf.getNumChannels() == 4);
        REQUIRE(buf.getNumFrames() == 512);
    }
}

TEST_CASE("AudioBuffer pointer access", "[AudioBuffer]")
{
    AudioBuffer buf(2, 256);
    
    SECTION("Write and read pointers are valid")
    {
        REQUIRE(buf.getWritePointer(0) != nullptr);
        REQUIRE(buf.getWritePointer(1) != nullptr);
        REQUIRE(buf.getReadPointer(0) != nullptr);
        REQUIRE(buf.getReadPointer(1) != nullptr);
    }
    
    SECTION("Invalid channel returns nullptr")
    {
        REQUIRE(buf.getWritePointer(-1) == nullptr);
        REQUIRE(buf.getWritePointer(2) == nullptr);
        REQUIRE(buf.getReadPointer(-1) == nullptr);
        REQUIRE(buf.getReadPointer(2) == nullptr);
    }
}

TEST_CASE("AudioBuffer data operations", "[AudioBuffer]")
{
    AudioBuffer buf(2, 128);
    
    SECTION("Write and verify data")
    {
        float* ch0 = buf.getWritePointer(0);
        ch0[0] = 0.5f;
        ch0[1] = -0.3f;
        
        const float* readCh0 = buf.getReadPointer(0);
        REQUIRE(readCh0[0] == Approx(0.5f));
        REQUIRE(readCh0[1] == Approx(-0.3f));
    }
    
    SECTION("Clear zeroes all data")
    {
        float* ch0 = buf.getWritePointer(0);
        ch0[0] = 0.7f;
        ch0[1] = 0.9f;
        
        buf.clear();
        
        const float* readCh0 = buf.getReadPointer(0);
        REQUIRE(readCh0[0] == Approx(0.0f));
        REQUIRE(readCh0[1] == Approx(0.0f));
    }
}
