#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "NorcoastLookAndFeel.h"

// Pill-style row of buttons for an APVTS choice parameter. One button
// per option, click to select. Uses the host param as source of truth
// so it stays in sync with automation / preset recall.
class ChoiceButtonRow : public juce::Component, private juce::Timer
{
public:
    ChoiceButtonRow (juce::AudioProcessorValueTreeState& s,
                     const juce::String& id,
                     juce::Colour accent = juce::Colour (NorcoastLookAndFeel::kAccent))
        : apvts (s), paramID (id), accentColour (accent)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        {
            const auto& names = choice->choices;
            for (int i = 0; i < names.size(); ++i)
            {
                auto* b = buttons.add (new juce::TextButton (names[i]));
                b->setColour (juce::TextButton::buttonColourId,
                              juce::Colour (NorcoastLookAndFeel::kPanelBg));
                b->setColour (juce::TextButton::buttonOnColourId, accent);
                b->setColour (juce::TextButton::textColourOnId,
                              juce::Colour (NorcoastLookAndFeel::kBg));
                b->setColour (juce::TextButton::textColourOffId, accent.withAlpha (0.6f));
                b->onClick = [this, i] { selectIndex (i); };
                addAndMakeVisible (b);
            }
        }
        startTimerHz (10);
    }

    ~ChoiceButtonRow() override { stopTimer(); }

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
        const int idx = (int) std::round (apvts.getRawParameterValue (paramID)->load());
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
    }

    void selectIndex (int i)
    {
        if (auto* p = apvts.getParameter (paramID))
        {
            const float n = p->getNormalisableRange().convertTo0to1 ((float) i);
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, n));
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramID;
    juce::Colour accentColour;
    juce::OwnedArray<juce::TextButton> buttons;
};
