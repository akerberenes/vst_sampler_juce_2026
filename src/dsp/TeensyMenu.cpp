#include "TeensyMenu.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

TeensyMenu::TeensyMenu()
{
    // Initialise each sample slot with NoEffect.
    for (int i = 0; i < NUM_SAMPLE_PAGES; ++i)
    {
        sampleEffects_[i].effectIndex = 0;
        sampleEffects_[i].paramValue = 0.5f;
        sampleEffects_[i].effect = EffectLibrary::createEffect(0);
    }
}

void TeensyMenu::setPageKnob(float value)
{
    // Quantise 0.0–1.0 into 5 equal bands: [0.0, 0.2) = page 0, etc.
    int page = static_cast<int>(std::clamp(value, 0.0f, 1.0f) * NUM_PAGES);
    if (page >= NUM_PAGES) page = NUM_PAGES - 1;
    currentPage_ = static_cast<Page>(page);
}

void TeensyMenu::setParamKnob(float value)
{
    float v = std::clamp(value, 0.0f, 1.0f);

    if (currentPage_ == Page::Preset)
    {
        // Preset page: 3 bands — save / reload / load other.
        int fn = static_cast<int>(v * NUM_PRESET_FUNCTIONS);
        if (fn >= NUM_PRESET_FUNCTIONS) fn = NUM_PRESET_FUNCTIONS - 1;
        auto newFn = static_cast<PresetFunction>(fn);
        // Activate pickup when switching into LoadOther so the value knob
        // doesn't immediately jump the destination preset.
        if (newFn == PresetFunction::LoadOther && selectedFunction_ != PresetFunction::LoadOther)
            destinationPickupPending_ = true;
        selectedFunction_ = newFn;
        return;
    }

    // Sample page: quantise to effect count.
    int sampleIdx = sampleIndexForCurrentPage();

    // Pickup guard: after reload, ignore input until knob reaches stored value.
    auto& pu = pickup_[sampleIdx];
    if (pu.paramPending)
    {
        float expected = getExpectedParamKnobValue();
        if (std::fabs(v - expected) > kPickupThreshold)
            return;
        pu.paramPending = false;
    }

    int numEffects = EffectLibrary::getEffectCount();
    int effectIdx = static_cast<int>(v * numEffects);
    if (effectIdx >= numEffects) effectIdx = numEffects - 1;

    auto& state = sampleEffects_[sampleIdx];
    if (state.effectIndex != effectIdx)
    {
        state.effectIndex = effectIdx;
        state.effect = EffectLibrary::createEffect(effectIdx);
        state.effect->setParamValue(state.paramValue);
        applyEffectToSampler(sampleIdx);
        dirty_ = true;
    }
}

void TeensyMenu::setParamValue(float value)
{
    if (currentPage_ == Page::Preset)
    {
        // On Preset page, value knob scrolls through destination presets
        // when "load other" is the selected function.
        if (selectedFunction_ == PresetFunction::LoadOther)
        {
            // Pickup guard: ignore input until the knob reaches the current
            // destination value, so switching to LoadOther doesn't jump.
            float expected = (destinationPreset_ + 0.5f) / static_cast<float>(NUM_PRESETS);
            if (destinationPickupPending_)
            {
                if (std::fabs(value - expected) > kPickupThreshold)
                    return;
                destinationPickupPending_ = false;
            }

            float v = std::clamp(value, 0.0f, 1.0f);
            int dest = static_cast<int>(v * NUM_PRESETS);
            if (dest >= NUM_PRESETS) dest = NUM_PRESETS - 1;
            destinationPreset_ = dest;
        }
        return;
    }

    int sampleIdx = sampleIndexForCurrentPage();

    // Pickup guard: after reload, ignore input until knob reaches stored value.
    auto& pu = pickup_[sampleIdx];
    if (pu.valuePending)
    {
        if (std::fabs(value - sampleEffects_[sampleIdx].paramValue) > kPickupThreshold)
            return;
        pu.valuePending = false;
    }

    auto& state = sampleEffects_[sampleIdx];
    state.paramValue = std::clamp(value, 0.0f, 1.0f);
    if (state.effect)
        state.effect->setParamValue(state.paramValue);
    dirty_ = true;
}

void TeensyMenu::triggerAction()
{
    if (currentPage_ != Page::Preset)
        return;

    if (selectedFunction_ == PresetFunction::Save)
    {
        // Snapshot current effect assignments.
        for (int i = 0; i < NUM_SAMPLE_PAGES; ++i)
        {
            savedPreset_[i].effectIndex = sampleEffects_[i].effectIndex;
            savedPreset_[i].paramValue  = sampleEffects_[i].paramValue;
        }
        if (onSave) onSave();
        dirty_ = false;
    }
    else if (selectedFunction_ == PresetFunction::Reload)
    {
        // Reload: restore effect assignments from snapshot.
        for (int i = 0; i < NUM_SAMPLE_PAGES; ++i)
        {
            setEffectForSample(i, savedPreset_[i].effectIndex, savedPreset_[i].paramValue);
            pickup_[i] = { true, true };
        }
        if (onReload) onReload();
        dirty_ = false;
    }
    else // LoadOther
    {
        if (onLoadPreset) onLoadPreset(destinationPreset_);
        // After loading, the host callback is responsible for calling
        // setEffectForSample() and setPresetName() to update our state.
        dirty_ = false;
    }
}

std::string TeensyMenu::getZoneText(int zone) const
{
    switch (zone)
    {
        case 0:  // Zone 1: page name (sample pages) or preset name (preset page)
        {
            if (currentPage_ == Page::Preset)
            {
                std::string display = presetName_;
                if (dirty_) display += "*";
                return fitToWidth(display, 8);
            }
            static const char* names[] = { "Sample1", "Sample2", "Sample3", "Sample4" };
            return fitToWidth(names[static_cast<int>(currentPage_)], 8);
        }

        case 1:  // Zone 2: preset name (sample pages) or destination (preset page)
        {
            if (currentPage_ == Page::Preset)
            {
                bool selected = (selectedFunction_ == PresetFunction::LoadOther);
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%sPreset%d", selected ? ">" : " ", destinationPreset_ + 1);
                return fitToWidth(buf, 8);
            }
            std::string display = presetName_;
            if (dirty_) display += "*";
            return fitToWidth(display, 8);
        }

        case 2:  // Zone 3: effect name (sample) or save (preset)
        {
            if (currentPage_ == Page::Preset)
            {
                bool selected = (selectedFunction_ == PresetFunction::Save);
                return fitToWidth(selected ? ">save" : " save", 8);
            }
            int sampleIdx = sampleIndexForCurrentPage();
            return fitToWidth(EffectLibrary::getEffectName(sampleEffects_[sampleIdx].effectIndex), 8);
        }

        case 3:  // Zone 4: effect param value (sample) or reload (preset)
        {
            if (currentPage_ == Page::Preset)
            {
                bool selected = (selectedFunction_ == PresetFunction::Reload);
                return fitToWidth(selected ? ">reload" : " reload", 8);
            }
            int sampleIdx = sampleIndexForCurrentPage();
            float val = sampleEffects_[sampleIdx].paramValue;
            char buf[9];
            std::snprintf(buf, sizeof(buf), "%3d%%", static_cast<int>(val * 100.0f));
            return fitToWidth(buf, 8);
        }

        default:
            return fitToWidth("", 8);
    }
}

std::string TeensyMenu::getRow(int row) const
{
    if (row == 0)
        return getZoneText(0) + getZoneText(1);
    else
        return getZoneText(2) + getZoneText(3);
}

int TeensyMenu::getSelectedEffectIndex(int sampleIndex) const
{
    if (sampleIndex < 0 || sampleIndex >= NUM_SAMPLE_PAGES)
        return 0;
    return sampleEffects_[sampleIndex].effectIndex;
}

float TeensyMenu::getEffectParamValue(int sampleIndex) const
{
    if (sampleIndex < 0 || sampleIndex >= NUM_SAMPLE_PAGES)
        return 0.0f;
    return sampleEffects_[sampleIndex].paramValue;
}

float TeensyMenu::getExpectedParamKnobValue() const
{
    if (currentPage_ == Page::Preset)
    {
        // 3 bands: centre of each third.
        int fn = static_cast<int>(selectedFunction_);
        return (fn + 0.5f) / static_cast<float>(NUM_PRESET_FUNCTIONS);
    }

    int idx = sampleEffects_[sampleIndexForCurrentPage()].effectIndex;
    int n   = EffectLibrary::getEffectCount();
    if (n <= 1) return 0.0f;
    return (idx + 0.5f) / static_cast<float>(n);
}

float TeensyMenu::getExpectedValueKnobValue() const
{
    if (currentPage_ == Page::Preset)
    {
        if (selectedFunction_ == PresetFunction::LoadOther)
            return (destinationPreset_ + 0.5f) / static_cast<float>(NUM_PRESETS);
        return 0.5f;
    }
    return sampleEffects_[sampleIndexForCurrentPage()].paramValue;
}

void TeensyMenu::setEffectForSample(int sampleIndex, int effectIndex, float paramValue)
{
    if (sampleIndex < 0 || sampleIndex >= NUM_SAMPLE_PAGES)
        return;

    int numEffects = EffectLibrary::getEffectCount();
    if (effectIndex < 0 || effectIndex >= numEffects)
        effectIndex = 0;

    auto& state = sampleEffects_[sampleIndex];
    state.effectIndex = effectIndex;
    state.paramValue  = std::clamp(paramValue, 0.0f, 1.0f);
    state.effect      = EffectLibrary::createEffect(effectIndex);
    state.effect->setParamValue(state.paramValue);
    applyEffectToSampler(sampleIndex);
}

int TeensyMenu::sampleIndexForCurrentPage() const
{
    if (currentPage_ == Page::Preset)
        return -1;
    return static_cast<int>(currentPage_);
}

void TeensyMenu::applyEffectToSampler(int sampleIndex)
{
    if (!samplerBank_ || sampleIndex < 0 || sampleIndex >= NUM_SAMPLE_PAGES)
        return;
    // Set the raw Effect pointer on the sampler. The TeensyMenu owns the unique_ptr.
    samplerBank_->getSample(sampleIndex).setEffect(sampleEffects_[sampleIndex].effect.get());
}

std::string TeensyMenu::fitToWidth(const std::string& str, int len)
{
    if (static_cast<int>(str.size()) >= len)
        return str.substr(0, len);
    // Pad with spaces on the right.
    return str + std::string(len - str.size(), ' ');
}
