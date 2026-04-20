#include <catch2/catch.hpp>
#include "TeensyMenu.h"
#include "SamplerBank.h"
#include "effects/EffectLibrary.h"

using namespace Catch;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Set the page knob to land on a specific page (0–4).
static void selectPage(TeensyMenu& m, int pageIndex)
{
    // Map page index to the centre of its 1/5 band to avoid boundary wobble.
    float v = (pageIndex + 0.5f) / static_cast<float>(TeensyMenu::NUM_PAGES);
    m.setPageKnob(v);
}

// Set the param knob to select a specific effect index.
static void selectEffect(TeensyMenu& m, int effectIndex)
{
    int numEffects = EffectLibrary::getEffectCount();
    float v = (effectIndex + 0.5f) / static_cast<float>(numEffects);
    m.setParamKnob(v);
}

// Zone text is exactly 8 characters.
static bool isWidth8(const std::string& s)
{
    return s.size() == 8;
}

// ---------------------------------------------------------------------------
// fitToWidth (tested indirectly through getZoneText, but explicitly here)
// ---------------------------------------------------------------------------
TEST_CASE("TeensyMenu zone text is always 8 characters", "[TeensyMenu][display]")
{
    TeensyMenu m;

    for (int page = 0; page < TeensyMenu::NUM_PAGES; ++page)
    {
        selectPage(m, page);
        for (int zone = 0; zone < 4; ++zone)
        {
            std::string text = m.getZoneText(zone);
            REQUIRE(isWidth8(text));
        }
    }
}

TEST_CASE("TeensyMenu getRow returns exactly 16 characters", "[TeensyMenu][display]")
{
    TeensyMenu m;

    for (int page = 0; page < TeensyMenu::NUM_PAGES; ++page)
    {
        selectPage(m, page);
        REQUIRE(m.getRow(0).size() == 16);
        REQUIRE(m.getRow(1).size() == 16);
    }
}

// ---------------------------------------------------------------------------
// Page selection
// ---------------------------------------------------------------------------
TEST_CASE("TeensyMenu page selection", "[TeensyMenu][page]")
{
    TeensyMenu m;

    SECTION("Default page is Sample1")
    {
        REQUIRE(m.getCurrentPage() == TeensyMenu::Page::Sample1);
    }

    SECTION("Sample pages 0–3 selected correctly")
    {
        selectPage(m, 0);
        REQUIRE(m.getCurrentPage() == TeensyMenu::Page::Sample1);

        selectPage(m, 1);
        REQUIRE(m.getCurrentPage() == TeensyMenu::Page::Sample2);

        selectPage(m, 2);
        REQUIRE(m.getCurrentPage() == TeensyMenu::Page::Sample3);

        selectPage(m, 3);
        REQUIRE(m.getCurrentPage() == TeensyMenu::Page::Sample4);
    }

    SECTION("Preset page selected correctly")
    {
        selectPage(m, 4);
        REQUIRE(m.getCurrentPage() == TeensyMenu::Page::Preset);
    }

    SECTION("Clamp: knob at 0.0 gives Sample1")
    {
        m.setPageKnob(0.0f);
        REQUIRE(m.getCurrentPage() == TeensyMenu::Page::Sample1);
    }

    SECTION("Clamp: knob at 1.0 gives Preset")
    {
        m.setPageKnob(1.0f);
        REQUIRE(m.getCurrentPage() == TeensyMenu::Page::Preset);
    }

    SECTION("Clamp: knob above 1.0 clamped to Preset")
    {
        m.setPageKnob(2.0f);
        REQUIRE(m.getCurrentPage() == TeensyMenu::Page::Preset);
    }
}

// ---------------------------------------------------------------------------
// Zone 0: page name
// ---------------------------------------------------------------------------
TEST_CASE("TeensyMenu zone 0 shows page name", "[TeensyMenu][display]")
{
    TeensyMenu m;

    selectPage(m, 0);
    REQUIRE(m.getZoneText(0).find("Sample1") != std::string::npos);

    selectPage(m, 1);
    REQUIRE(m.getZoneText(0).find("Sample2") != std::string::npos);

    selectPage(m, 2);
    REQUIRE(m.getZoneText(0).find("Sample3") != std::string::npos);

    selectPage(m, 3);
    REQUIRE(m.getZoneText(0).find("Sample4") != std::string::npos);

    selectPage(m, 4);
    REQUIRE(m.getZoneText(0).find("Preset") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Zone 1: preset name
// ---------------------------------------------------------------------------
TEST_CASE("TeensyMenu zone 1 shows preset name", "[TeensyMenu][display]")
{
    TeensyMenu m;

    SECTION("Default preset name")
    {
        REQUIRE(m.getZoneText(1).find("Preset1") != std::string::npos);
    }

    SECTION("Custom preset name")
    {
        m.setPresetName("Song01");
        REQUIRE(m.getZoneText(1).find("Song01") != std::string::npos);
    }

    SECTION("Preset name truncated to 8 chars")
    {
        m.setPresetName("VeryLongPresetName");
        REQUIRE(isWidth8(m.getZoneText(1)));
    }

    SECTION("Zone 1 shows preset name on every page")
    {
        m.setPresetName("Test");
        // Preset page (last page) uses zone 1 for destination, not preset name.
        for (int page = 0; page < TeensyMenu::NUM_PAGES - 1; ++page)
        {
            selectPage(m, page);
            REQUIRE(m.getZoneText(1).find("Test") != std::string::npos);
        }
    }
}

// ---------------------------------------------------------------------------
// Effect selection on sample pages
// ---------------------------------------------------------------------------
TEST_CASE("TeensyMenu effect selection on sample pages", "[TeensyMenu][effects]")
{
    TeensyMenu m;
    selectPage(m, 0);  // Sample1

    SECTION("Default effect is None (index 0)")
    {
        REQUIRE(m.getSelectedEffectIndex(0) == 0);
        REQUIRE(m.getZoneText(2).find("None") != std::string::npos);
    }

    SECTION("Select each effect in turn")
    {
        int numEffects = EffectLibrary::getEffectCount();
        for (int i = 0; i < numEffects; ++i)
        {
            selectEffect(m, i);
            REQUIRE(m.getSelectedEffectIndex(0) == i);
            REQUIRE(m.getZoneText(2).find(EffectLibrary::getEffectName(i)) != std::string::npos);
        }
    }

    SECTION("Effect indices are independent per sample")
    {
        selectPage(m, 0);
        selectEffect(m, 1);  // Distortion on sample 0

        selectPage(m, 1);
        selectEffect(m, 2);  // BitCrush on sample 1

        REQUIRE(m.getSelectedEffectIndex(0) == 1);
        REQUIRE(m.getSelectedEffectIndex(1) == 2);
    }

    SECTION("Clamp: param knob at 0.0 selects effect 0")
    {
        m.setParamKnob(0.0f);
        REQUIRE(m.getSelectedEffectIndex(0) == 0);
    }

    SECTION("Clamp: param knob at 1.0 selects last effect")
    {
        m.setParamKnob(1.0f);
        int lastIdx = EffectLibrary::getEffectCount() - 1;
        REQUIRE(m.getSelectedEffectIndex(0) == lastIdx);
    }
}

// ---------------------------------------------------------------------------
// Zone 3: effect param value display on sample pages
// ---------------------------------------------------------------------------
TEST_CASE("TeensyMenu zone 3 shows effect param value on sample pages", "[TeensyMenu][display]")
{
    TeensyMenu m;
    selectPage(m, 0);

    SECTION("Default param is 50%")
    {
        REQUIRE(m.getEffectParamValue(0) == Approx(0.5f));
        REQUIRE(m.getZoneText(3).find("50%") != std::string::npos);
    }

    SECTION("Set param to 0 shows 0%")
    {
        m.setParamValue(0.0f);
        REQUIRE(m.getZoneText(3).find("0%") != std::string::npos);
    }

    SECTION("Set param to 1.0 shows 100%")
    {
        m.setParamValue(1.0f);
        REQUIRE(m.getZoneText(3).find("100%") != std::string::npos);
    }

    SECTION("Param value clamped above 1.0")
    {
        m.setParamValue(2.0f);
        REQUIRE(m.getEffectParamValue(0) == Approx(1.0f));
    }

    SECTION("Param value clamped below 0.0")
    {
        m.setParamValue(-1.0f);
        REQUIRE(m.getEffectParamValue(0) == Approx(0.0f));
    }

    SECTION("Param values are independent per sample")
    {
        selectPage(m, 0);
        m.setParamValue(0.2f);

        selectPage(m, 2);
        m.setParamValue(0.8f);

        REQUIRE(m.getEffectParamValue(0) == Approx(0.2f));
        REQUIRE(m.getEffectParamValue(2) == Approx(0.8f));
    }

    SECTION("setParamValue on Preset page does nothing")
    {
        m.setParamValue(0.3f);
        float before = m.getEffectParamValue(0);

        selectPage(m, 4);  // Preset page
        m.setParamValue(0.9f);

        selectPage(m, 0);
        REQUIRE(m.getEffectParamValue(0) == Approx(before));
    }
}

// ---------------------------------------------------------------------------
// Preset page display and function selection
// ---------------------------------------------------------------------------
TEST_CASE("TeensyMenu preset page display", "[TeensyMenu][preset]")
{
    TeensyMenu m;
    selectPage(m, 4);  // Preset page

    SECTION("Default selected function is Save (index 0)")
    {
        REQUIRE(m.getSelectedPresetFunction() == 0);
    }

    SECTION("Zone 2 shows >save when Save is selected")
    {
        m.setParamKnob(0.0f);  // Save
        REQUIRE(m.getSelectedPresetFunction() == 0);
        std::string z2 = m.getZoneText(2);
        REQUIRE(z2.find(">save") != std::string::npos);
        REQUIRE(z2.find(">reload") == std::string::npos);
    }

    SECTION("Zone 3 shows >reload when Reload is selected")
    {
        m.setParamKnob(0.5f);  // Reload (band 1 of 3)
        REQUIRE(m.getSelectedPresetFunction() == 1);
        std::string z3 = m.getZoneText(3);
        REQUIRE(z3.find(">reload") != std::string::npos);
    }

    SECTION("Save: zone 2 has cursor, zone 3 has no cursor")
    {
        m.setParamKnob(0.2f);  // Save half
        REQUIRE(m.getZoneText(2).find(">save")   != std::string::npos);
        REQUIRE(m.getZoneText(3).find(">reload") == std::string::npos);
        REQUIRE(m.getZoneText(3).find(" reload") != std::string::npos);
    }

    SECTION("Reload: zone 3 has cursor, zone 2 has no cursor")
    {
        m.setParamKnob(0.5f);  // Reload (band 1 of 3)
        REQUIRE(m.getZoneText(3).find(">reload") != std::string::npos);
        REQUIRE(m.getZoneText(2).find(">save")   == std::string::npos);
        REQUIRE(m.getZoneText(2).find(" save")   != std::string::npos);
    }

    SECTION("Exactly one zone has cursor at a time")
    {
        m.setParamKnob(0.1f);
        bool z2HasCursor = m.getZoneText(2)[0] == '>';
        bool z3HasCursor = m.getZoneText(3)[0] == '>';
        REQUIRE((z2HasCursor ^ z3HasCursor) == true);  // XOR: exactly one
    }

    SECTION("Boundary: knob exactly at 0.5 → Reload")
    {
        m.setParamKnob(0.5f);
        REQUIRE(m.getSelectedPresetFunction() == 1);
    }

    SECTION("Param knob on preset page does not affect sample effects")
    {
        selectPage(m, 0);
        selectEffect(m, 1);
        int effectBefore = m.getSelectedEffectIndex(0);

        selectPage(m, 4);
        m.setParamKnob(0.9f);  // Changes preset function, not effect

        REQUIRE(m.getSelectedEffectIndex(0) == effectBefore);
    }
}

// ---------------------------------------------------------------------------
// Row assembly
// ---------------------------------------------------------------------------
TEST_CASE("TeensyMenu row assembly", "[TeensyMenu][display]")
{
    TeensyMenu m;

    SECTION("Row 0 = zone 0 + zone 1")
    {
        selectPage(m, 0);
        std::string row0 = m.getRow(0);
        REQUIRE(row0 == m.getZoneText(0) + m.getZoneText(1));
    }

    SECTION("Row 1 = zone 2 + zone 3")
    {
        selectPage(m, 0);
        std::string row1 = m.getRow(1);
        REQUIRE(row1 == m.getZoneText(2) + m.getZoneText(3));
    }
}

// ---------------------------------------------------------------------------
// Effect applied to sampler
// ---------------------------------------------------------------------------
TEST_CASE("TeensyMenu applies effect to sampler via SamplerBank", "[TeensyMenu][effects]")
{
    SamplerBank bank;
    bank.prepare(48000, 256);

    TeensyMenu m;
    m.setSamplerBank(&bank);

    SECTION("Selecting an effect: sampler effect pointer is non-null")
    {
        selectPage(m, 0);
        selectEffect(m, 1);  // Distortion — changes from default, triggers applyEffectToSampler
        REQUIRE(bank.getSample(0).getEffect() != nullptr);
    }

    SECTION("Switching effect changes the pointer on the correct sampler")
    {
        selectPage(m, 0);
        selectEffect(m, 1);  // Distortion
        Effect* e0 = bank.getSample(0).getEffect();
        REQUIRE(e0 != nullptr);
        REQUIRE(std::string(e0->getName()) == "Distort");

        // Sample 1 is still on NoEffect.
        REQUIRE(bank.getSample(1).getEffect() == nullptr);
    }

    SECTION("Each sample pad gets its own effect")
    {
        selectPage(m, 0); selectEffect(m, 1);  // Sample0 → Distortion
        selectPage(m, 1); selectEffect(m, 2);  // Sample1 → BitCrush
        selectPage(m, 2); selectEffect(m, 3);  // Sample2 → Filter
        selectPage(m, 3); selectEffect(m, 0);  // Sample3 → None

        REQUIRE(std::string(bank.getSample(0).getEffect()->getName()) == "Distort");
        REQUIRE(std::string(bank.getSample(1).getEffect()->getName()) == "BitCrush");
        REQUIRE(std::string(bank.getSample(2).getEffect()->getName()) == "Filter");
    }

    SECTION("Without setSamplerBank, selecting effects does not crash")
    {
        TeensyMenu m2;  // No bank connected.
        selectPage(m2, 0);
        REQUIRE_NOTHROW(selectEffect(m2, 1));
    }
}

// ---------------------------------------------------------------------------
// Effect param value is forwarded to the effect object
// ---------------------------------------------------------------------------
TEST_CASE("TeensyMenu forwards param value to Effect object", "[TeensyMenu][effects]")
{
    TeensyMenu m;
    selectPage(m, 0);
    selectEffect(m, 1);  // Distortion

    m.setParamValue(0.75f);
    REQUIRE(m.getEffectParamValue(0) == Approx(0.75f));

    // Switching to a new effect should preserve the stored param value.
    selectEffect(m, 2);  // BitCrush — same stored paramValue applied on creation
    REQUIRE(m.getEffectParamValue(0) == Approx(0.75f));
}

// ---------------------------------------------------------------------------
// Out-of-range accessors
// ---------------------------------------------------------------------------
TEST_CASE("TeensyMenu out-of-range accessors are safe", "[TeensyMenu][safety]")
{
    TeensyMenu m;

    SECTION("getSelectedEffectIndex out of range returns 0")
    {
        REQUIRE(m.getSelectedEffectIndex(-1) == 0);
        REQUIRE(m.getSelectedEffectIndex(4) == 0);
    }

    SECTION("getEffectParamValue out of range returns 0.0")
    {
        REQUIRE(m.getEffectParamValue(-1) == Approx(0.0f));
        REQUIRE(m.getEffectParamValue(4) == Approx(0.0f));
    }

    SECTION("getZoneText with invalid zone returns 8-space string")
    {
        std::string text = m.getZoneText(99);
        REQUIRE(text.size() == 8);
        REQUIRE(text == "        ");
    }
}
