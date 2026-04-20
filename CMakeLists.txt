cmake_minimum_required(VERSION 3.24)
project(SamplerWithFreeze VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE configuration
set(JUCE_BUILD_HELPER_TOOLS OFF)
add_subdirectory("C:/JUCE" "${CMAKE_BINARY_DIR}/JUCE")

# DSP Module Library (platform-agnostic, no JUCE)
add_library(SamplerDSP STATIC
    src/dsp/CircularBuffer.cpp
    src/dsp/Sampler.cpp
    src/dsp/SamplerBank.cpp
    src/dsp/Mixer.cpp
    src/dsp/FreezeEffect.cpp
    src/dsp/AudioBuffer.cpp
)
target_include_directories(SamplerDSP PUBLIC src/dsp)
target_compile_features(SamplerDSP PRIVATE cxx_std_17)

# Audio File Handling Library
add_library(AudioFileHandling STATIC
    src/audio/AudioFileLoader.cpp
    src/audio/PresetManager.cpp
)
target_include_directories(AudioFileHandling PUBLIC src/audio)
target_link_libraries(AudioFileHandling PRIVATE SamplerDSP)
target_compile_features(AudioFileHandling PRIVATE cxx_std_17)

# JUCE Plugin
juce_add_plugin(SamplerWithFreeze
    COMPANY_NAME "MusicProjects"
    PLUGIN_MANUFACTURER_CODE "MPRO"
    PLUGIN_CODE "SAM1"
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Sampler With Freeze"
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "C:/JUCE/plugins/vst"
)

target_sources(SamplerWithFreeze PRIVATE
    src/juce/PluginProcessor.cpp
    src/juce/PluginEditor.cpp
    src/juce/WaveformDisplay.cpp
    src/juce/SampleTabPanel.cpp
    src/juce/FreezeBufferDisplay.cpp
)

target_compile_features(SamplerWithFreeze PRIVATE cxx_std_17)
target_compile_definitions(SamplerWithFreeze PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
)
target_include_directories(SamplerWithFreeze PRIVATE src)
target_link_libraries(SamplerWithFreeze PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_utils
    SamplerDSP
    AudioFileHandling
)

# Unit Tests (using Catch2 header-only)
# Add tests subdirectory if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit_tests/CMakeLists.txt")
    add_subdirectory(unit_tests)
endif()
