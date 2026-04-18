#pragma once

#include <string>

/**
 * PresetManager
 * 
 * Handles saving and loading plugin state as presets.
 * Includes sample data embedding for portability.
 */
class PresetManager
{
public:
    struct Preset
    {
        std::string name;
        // TODO: add all preset data (samples, parameters, etc)
    };
    
    bool savePreset(const std::string& filePath, const Preset& preset);
    bool loadPreset(const std::string& filePath, Preset& outPreset);
    
private:
    // Implementation (TODO)
};
