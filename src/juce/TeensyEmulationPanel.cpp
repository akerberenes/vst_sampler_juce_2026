#include "TeensyEmulationPanel.h"

TeensyEmulationPanel::TeensyEmulationPanel(juce::AudioProcessorValueTreeState& apvts,
                                           TeensyMenu& menu)
    : menu_(menu)
{
    // --- LCD display starts blank ---
    lcdRow0_ = menu_.getRow(0);
    lcdRow1_ = menu_.getRow(1);

    // --- Page knob (selects which page: Sample1–4 or Preset) ---
    addAndMakeVisible(pageLabel_);
    pageLabel_.setText("Page", juce::dontSendNotification);
    pageLabel_.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(pageKnob_);
    pageKnob_.setSliderStyle(juce::Slider::Rotary);
    pageKnob_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    pageAttachment_ = std::make_unique<SliderAttachment>(apvts, "teensyPage", pageKnob_);

    // --- Param knob (selects effect or save/reload) ---
    addAndMakeVisible(paramLabel_);
    paramLabel_.setText("Param", juce::dontSendNotification);
    paramLabel_.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(paramKnob_);
    paramKnob_.setSliderStyle(juce::Slider::Rotary);
    paramKnob_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    paramAttachment_ = std::make_unique<SliderAttachment>(apvts, "teensyParam", paramKnob_);

    // --- Value knob (sets effect parameter value) ---
    addAndMakeVisible(valueLabel_);
    valueLabel_.setText("Value", juce::dontSendNotification);
    valueLabel_.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(valueKnob_);
    valueKnob_.setSliderStyle(juce::Slider::Rotary);
    valueKnob_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    valueAttachment_ = std::make_unique<SliderAttachment>(apvts, "teensyValue", valueKnob_);

    // --- Action button ---
    addAndMakeVisible(actionButton_);
    actionButton_.setButtonText("ACTION");
    actionButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a5a));
    actionButton_.onClick = [this]
    {
        menu_.triggerAction();
        // After reload, snap knobs to match restored state so the pickup
        // condition is satisfied immediately (VST knobs follow values).
        paramKnob_.setValue(static_cast<double>(menu_.getExpectedParamKnobValue()),
                            juce::dontSendNotification);
        valueKnob_.setValue(static_cast<double>(menu_.getExpectedValueKnobValue()),
                            juce::dontSendNotification);
    };

    // Poll the menu state 15 times per second to update the LCD.
    startTimerHz(15);
}

TeensyEmulationPanel::~TeensyEmulationPanel()
{
    stopTimer();
}

void TeensyEmulationPanel::timerCallback()
{
    // Feed the page knob first so we can detect page changes.
    menu_.setPageKnob(static_cast<float>(pageKnob_.getValue()));

    // If the page just changed, sync the param & value knobs to the new page's
    // stored state BEFORE they are fed back into the menu.  This prevents the
    // old knob positions from overwriting the new page's effect assignment.
    TeensyMenu::Page newPage = menu_.getCurrentPage();
    if (newPage != cachedPage_)
    {
        paramKnob_.setValue(static_cast<double>(menu_.getExpectedParamKnobValue()),
                            juce::dontSendNotification);
        valueKnob_.setValue(static_cast<double>(menu_.getExpectedValueKnobValue()),
                            juce::dontSendNotification);
    }

    menu_.setParamKnob(static_cast<float>(paramKnob_.getValue()));
    menu_.setParamValue(static_cast<float>(valueKnob_.getValue()));

    // Update cached LCD text and state.
    std::string row0 = menu_.getRow(0);
    std::string row1 = menu_.getRow(1);
    TeensyMenu::Page page = menu_.getCurrentPage();
    int fn = menu_.getSelectedPresetFunction();
    if (row0 != lcdRow0_ || row1 != lcdRow1_ || page != cachedPage_ || fn != cachedPresetFunction_)
    {
        lcdRow0_ = row0;
        lcdRow1_ = row1;
        cachedPage_ = page;
        cachedPresetFunction_ = fn;
        repaint();
    }
}

void TeensyEmulationPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // --- Section header ---
    g.setColour(juce::Colours::grey);
    g.setFont(12.0f);
    g.drawText("TEENSY UI EMULATION", bounds.removeFromTop(18),
               juce::Justification::centredLeft);

    // --- LCD background ---
    int lcdX = 10;
    int lcdY = 22;
    int lcdW = bounds.getWidth() - 20;
    int lcdH = 48;

    g.setColour(juce::Colour(0xff0a1a0a));  // Very dark green/black.
    g.fillRoundedRectangle((float)lcdX, (float)lcdY, (float)lcdW, (float)lcdH, 4.0f);

    // --- LCD border ---
    g.setColour(juce::Colour(0xff224422));
    g.drawRoundedRectangle((float)lcdX, (float)lcdY, (float)lcdW, (float)lcdH, 4.0f, 1.5f);

    // --- LCD text (monospace, green on dark) ---
    // Draw zone by zone so we can apply negative-character highlighting.
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain));

    int textPad = 8;
    int rowH = lcdH / 2;
    int zoneW = (lcdW - 2 * textPad) / 2;

    // Zone index layout:
    //   Row 0: zone 0 (left), zone 1 (right)
    //   Row 1: zone 2 (left = "save"), zone 3 (right = "reload")
    bool onPreset = (cachedPage_ == TeensyMenu::Page::Preset);

    for (int z = 0; z < 4; ++z)
    {
        int zx = lcdX + textPad + (z % 2) * zoneW;
        int zy = lcdY + (z / 2) * rowH;
        std::string zoneStr = menu_.getZoneText(z);

        // Determine if this zone should be highlighted (negative characters).
        bool highlight = false;
        if (onPreset)
        {
            // fn 0=Save highlights zone 2, fn 1=Reload highlights zone 3,
            // fn 2=LoadOther highlights zone 1 (destination).
            highlight = (z == 2 && cachedPresetFunction_ == 0)
                     || (z == 3 && cachedPresetFunction_ == 1)
                     || (z == 1 && cachedPresetFunction_ == 2);
        }

        if (highlight)
        {
            // Negative: bright green fill, black text.
            g.setColour(juce::Colour(0xff33ff33));
            g.fillRect(zx, zy + 2, zoneW, rowH - 4);
            g.setColour(juce::Colours::black);
        }
        else
        {
            g.setColour(juce::Colour(0xff33ff33));  // Bright green text.
        }

        g.drawText(juce::String(zoneStr), zx, zy, zoneW, rowH, juce::Justification::centredLeft);
    }

    // --- Zone separator (subtle vertical line in the middle of each row) ---
    float midX = (float)(lcdX + textPad + zoneW);
    g.setColour(juce::Colour(0xff1a3a1a));
    g.drawLine(midX, (float)lcdY + 2, midX, (float)(lcdY + lcdH) - 2, 1.0f);
}

void TeensyEmulationPanel::resized()
{
    // Layout: LCD display on top, 3 knobs + action button below.
    int lcdH = 48;
    int headerH = 22;
    int controlsY = headerH + lcdH + 8;
    int controlH = getHeight() - controlsY;
    int pad = 6;

    // Divide horizontal space into 4 equal columns: Page | Param | Value | Action.
    int totalW = getWidth() - pad * 2;
    int colW = totalW / 4;
    int labelH = 14;
    int knobH = controlH - labelH - 2;
    if (knobH < 20) knobH = 20;

    int x = pad;

    pageLabel_.setBounds(x, controlsY, colW, labelH);
    pageKnob_.setBounds(x, controlsY + labelH, colW, knobH);
    x += colW;

    paramLabel_.setBounds(x, controlsY, colW, labelH);
    paramKnob_.setBounds(x, controlsY + labelH, colW, knobH);
    x += colW;

    valueLabel_.setBounds(x, controlsY, colW, labelH);
    valueKnob_.setBounds(x, controlsY + labelH, colW, knobH);
    x += colW;

    actionButton_.setBounds(x + 4, controlsY + labelH + (knobH - 28) / 2, colW - 8, 28);
}
