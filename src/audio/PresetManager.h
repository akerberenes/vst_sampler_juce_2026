#pragma once

#include <string>

/**
 * PresetManager
 *
 * Skeleton for saving and loading plugin presets.
 *
 * --- What a preset stores ---
 * A preset captures the full plugin state so that a session can be reproduced
 * exactly: which sample files are loaded on each pad, their start/end markers,
 * loop mode, freeze parameters, mixer levels, etc.
 *
 * --- Planned preset format ---
 * For the VST3 build, the most natural approach is to embed preset data inside
 * JUCE's AudioProcessorValueTreeState (APVTS) state, serialised as XML.
 * PluginProcessor's getStateInformation/setStateInformation already handles the
 * parameter half; PresetManager would add named preset slots on top.
 *
 * For the Teensy port a simpler binary format stored on SD card is planned.
 *
 * --- Current state ---
 * All methods are stubs. Presets are not saved or loaded yet.
 * The `Preset` struct will be expanded as the feature is implemented.
 */
class PresetManager
{
public:
    // Holds the data for one preset slot.
    // TODO: expand with sample paths, all APVTS parameter values, etc.
    struct Preset
    {
        std::string name;  // Display name shown in the preset browser.
        // TODO: add all preset data (samples, parameters, etc)
    };

    // Serialise `preset` and write it to `filePath`.
    // Returns true on success, false on any I/O or serialisation error.
    bool savePreset(const std::string& filePath, const Preset& preset);

    // Read and deserialise a preset from `filePath`.
    // Returns true and populates `outPreset` on success.
    bool loadPreset(const std::string& filePath, Preset& outPreset);

private:
    // Serialisation helpers (TODO).
    // e.g. std::string serialise(const Preset&);
    //      bool deserialise(const std::string& data, Preset&);
};
