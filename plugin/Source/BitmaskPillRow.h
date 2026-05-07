#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "NorcoastLookAndFeel.h"

// Pill-style row of buttons backed by an APVTS int bitmask. Each pill
// toggles its corresponding bit. Used for the chord-evolve pool, drum
// custom mask, etc.
//
// `alwaysOnBit` is an optional bit index (e.g. 0 for the chord root)
// that the user can't toggle off — useful for "this degree is always
// part of the chord" cases.
class BitmaskPillRow : public juce::Component, private juce::Timer
{
public:
    BitmaskPillRow (juce::AudioProcessorValueTreeState& s,
                    const juce::String& id,
                    const juce::StringArray& labels,
                    juce::Colour accent = juce::Colour (NorcoastLookAndFeel::kAccent),
                    int alwaysOnBitIndex = -1)
        : apvts (s), paramID (id), alwaysOnBit (alwaysOnBitIndex)
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (labels[i]));
            b->setColour (juce::TextButton::buttonColourId,
                          juce::Colour (NorcoastLookAndFeel::kPanelBg));
            b->setColour (juce::TextButton::buttonOnColourId, accent);
            b->setColour (juce::TextButton::textColourOnId,
                          juce::Colour (NorcoastLookAndFeel::kBg));
            // Web-app pill style: dim white when off, accent on the
            // active state.
            b->setColour (juce::TextButton::textColourOffId,
                          juce::Colour (0xffd6dae6).withAlpha (0.55f));
            b->onClick = [this, i] { toggleBit (i); };
            addAndMakeVisible (b);
        }
        startTimerHz (10);
    }

    ~BitmaskPillRow() override { stopTimer(); }

    // Allow callers to relabel a pill at runtime (used for the Custom
    // Chord row where each degree's note name follows the home root).
    void setLabel (int i, const juce::String& text)
    {
        if (auto* b = buttons[i])
            if (b->getButtonText() != text)
                b->setButtonText (text);
    }

    void resized() override
    {
        if (buttons.isEmpty()) return;
        const int n = buttons.size();
        const int w = getWidth() / n;
        for (int i = 0; i < n; ++i)
            buttons[i]->setBounds (i * w, 0, w - 2, getHeight());
    }

private:
    void timerCallback() override
    {
        const int mask = (int) apvts.getRawParameterValue (paramID)->load();
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (
                i == alwaysOnBit || (mask & (1 << i)) != 0,
                juce::dontSendNotification);
    }

    void toggleBit (int i)
    {
        if (i == alwaysOnBit) return;
        if (auto* p = apvts.getParameter (paramID))
        {
            const int mask    = (int) apvts.getRawParameterValue (paramID)->load();
            const int newMask = mask ^ (1 << i);
            const auto range  = p->getNormalisableRange();
            p->setValueNotifyingHost (range.convertTo0to1 ((float) newMask));
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramID;
    int alwaysOnBit;
    juce::OwnedArray<juce::TextButton> buttons;
};
