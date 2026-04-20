#pragma once

#include "effects/EffectLibrary.h"
#include "SamplerBank.h"
#include <string>
#include <array>
#include <memory>
#include <functional>

/**
 * TeensyMenu
 *
 * Portable state machine modelling the Teensy hardware UI.
 * Manages 5 pages (Sample1–4, Preset) and 4 display zones that map to
 * a 2-row × 16-column LCD (4 zones of 8 characters each).
 *
 * --- Inputs (all normalised 0.0–1.0) ---
 *   pageKnob   : selects the current page (quantised to 5 bands).
 *   paramKnob  : on sample pages, selects the effect; on preset page, selects save/reload.
 *   paramValue : on sample pages, sets the effect parameter value.
 *   actionButton : on preset page, executes the selected function.
 *
 * --- Display zones ---
 *   Zone 1 (row 0, col 0–7)  : page name
 *   Zone 2 (row 0, col 8–15) : preset name
 *   Zone 3 (row 1, col 0–7)  : context-dependent (effect name or "save")
 *   Zone 4 (row 1, col 8–15) : context-dependent (effect value or "reload")
 *
 * No JUCE dependencies — pure C++17.
 */
class TeensyMenu
{
public:
    static constexpr int NUM_PAGES = 5;
    static constexpr int NUM_SAMPLE_PAGES = 4;

    enum class Page { Sample1 = 0, Sample2, Sample3, Sample4, Preset };

    TeensyMenu();
    ~TeensyMenu() = default;

    // Connect the menu to the sampler bank so effect assignments take effect.
    void setSamplerBank(SamplerBank* bank) { samplerBank_ = bank; }

    // --- Input handling (called from JUCE UI or Teensy hardware) ---

    // Set the page selection knob (0.0–1.0, quantised to 5 bands).
    void setPageKnob(float value);

    // Set the parameter selection knob (0.0–1.0).
    // On sample pages: quantised to effect count.
    // On preset page: <0.5 = save, >=0.5 = reload.
    void setParamKnob(float value);

    // Set the parameter value knob (0.0–1.0).
    // On sample pages: sets the selected effect's parameter.
    void setParamValue(float value);

    // Trigger the action button.
    // On preset page: executes save or reload.
    void triggerAction();

    // --- Display output ---

    // Get the text for one of the 4 display zones (0–3).
    // Each returns a string of at most 8 characters.
    std::string getZoneText(int zone) const;

    // Convenience: get the full 2×16 display as two 16-char strings.
    std::string getRow(int row) const;

    // --- State queries ---

    Page getCurrentPage() const { return currentPage_; }
    int getSelectedEffectIndex(int sampleIndex) const;
    float getEffectParamValue(int sampleIndex) const;
    const std::string& getPresetName() const { return presetName_; }
    // Returns 0=Save, 1=Reload, 2=LoadOther.
    int getSelectedPresetFunction() const { return static_cast<int>(selectedFunction_); }
    int getDestinationPreset() const { return destinationPreset_; }
    void setPresetName(const std::string& name) { presetName_ = name; }

    // Dirty flag: true when state has changed since last save.
    bool isDirty() const { return dirty_; }
    void setDirty(bool d) { dirty_ = d; }

    // Optional callbacks fired by triggerAction() so the host layer (JUCE/Teensy)
    // can save/restore additional state beyond the per-sample effects.
    std::function<void()> onSave;
    std::function<void()> onReload;
    std::function<void(int)> onLoadPreset;  // Called with destination preset index (0-based).

    // --- Knob reverse-mapping (returns the knob position that matches current state) ---

    // Returns the paramKnob float (0–1) that selects the current effect/function.
    float getExpectedParamKnobValue() const;
    // Returns the valueKnob float (0–1) that matches the current param value.
    float getExpectedValueKnobValue() const;

    // --- Direct state access (for JUCE persistence) ---

    // Set the effect for a specific sample slot (recreates the Effect object).
    void setEffectForSample(int sampleIndex, int effectIndex, float paramValue);

private:
    // Current page (derived from pageKnob).
    Page currentPage_ = Page::Sample1;

    // Per-sample effect state.
    struct SampleEffectState
    {
        int effectIndex = 0;                        // Index into EffectLibrary.
        float paramValue = 0.5f;                    // Normalised effect parameter.
        std::unique_ptr<Effect> effect;              // Owned effect instance.
    };
    std::array<SampleEffectState, NUM_SAMPLE_PAGES> sampleEffects_;

    // Preset page state.
    enum class PresetFunction { Save = 0, Reload = 1, LoadOther = 2 };
    static constexpr int NUM_PRESET_FUNCTIONS = 3;
    static constexpr int NUM_PRESETS = 8;
    PresetFunction selectedFunction_ = PresetFunction::Save;
    int destinationPreset_ = 0;  // 0-based index into preset slots.
    bool destinationPickupPending_ = false;  // Pickup guard for destination knob.
    std::string presetName_ = "Preset1";
    bool dirty_ = false;

    // Saved preset snapshot (effectIndex + paramValue per sample pad).
    struct PresetSnapshot { int effectIndex = 0; float paramValue = 0.5f; };
    std::array<PresetSnapshot, NUM_SAMPLE_PAGES> savedPreset_;

    // Non-owning pointer to the SamplerBank (for assigning effects to samplers).
    SamplerBank* samplerBank_ = nullptr;

    // Pickup mode: after a preset reload, knob inputs are ignored per-sample
    // until the incoming value is close enough to the stored value.
    // On hardware (Teensy) the user must physically turn the knob to "pick up"
    // the stored value.  On the VST the panel snaps knobs instantly, so pickup
    // is satisfied on the first tick.
    static constexpr float kPickupThreshold = 0.05f;
    struct PickupState { bool paramPending = false; bool valuePending = false; };
    std::array<PickupState, NUM_SAMPLE_PAGES> pickup_{};

    // Apply the current effect to the sampler for the given page index.
    void applyEffectToSampler(int sampleIndex);

    // Returns the sample index for the current page, or -1 on the Preset page.
    int sampleIndexForCurrentPage() const;

    // Truncate or pad a string to exactly `len` characters.
    static std::string fitToWidth(const std::string& str, int len);
};
