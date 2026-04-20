cmake_minimum_required(VERSION 3.24)

# Test executable using Catch2
add_executable(DSPTests
    main.cpp
    test_AudioBuffer.cpp
    test_CircularBuffer.cpp
    test_Sampler.cpp
    test_SamplerBank.cpp
    test_Mixer.cpp
    test_FreezeEffect.cpp
    test_Integration.cpp
    catch2/catch_amalgamated.cpp
)

target_compile_features(DSPTests PRIVATE cxx_std_17)

# Link against SamplerDSP library (already built by parent CMakeLists.txt)
target_link_libraries(DSPTests PRIVATE SamplerDSP)

# Include Catch2 (header-only mode)
# Download Catch2 single header if not present
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/catch2/catch.hpp")
    message(STATUS "Downloading Catch2 single header...")
    file(DOWNLOAD 
        "https://github.com/catchorg/Catch2/releases/download/v3.4.0/catch_amalgamated.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/catch2/catch.hpp"
        TIMEOUT 30
        SHOW_PROGRESS
    )
endif()

target_include_directories(DSPTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(DSPTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/catch2")
target_include_directories(DSPTests PRIVATE "${CMAKE_SOURCE_DIR}/src/dsp")

# Add as test
enable_testing()
add_test(NAME DSPTests COMMAND DSPTests)
