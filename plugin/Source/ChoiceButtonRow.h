#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "NorcoastLookAndFeel.h"

// Pill-style row of buttons for an APVTS choice parameter. One button
// per option, click to select. Uses the host param as source of truth
// so it stays in sync with automation / preset recall.
class ChoiceButtonRow : public juce::Component, private juce::Timer
{
public:
    // skipFirstN — start the visible button list at index `skipFirstN`
    // (used to hide a leading "Off" choice when the layer has its own
    // mute / volume fader).
    ChoiceButtonRow (juce::AudioProcessorValueTreeState& s,
                     const juce::String& id,
                     juce::Colour accent = juce::Colour (NorcoastLookAndFeel::kAccent),
                     int rowsHint = 1,
                     int skipFirstN = 0)
        : apvts (s), paramID (id), accentColour (accent),
          numRows (juce::jmax (1, rowsHint)),
          firstVisible (juce::jmax (0, skipFirstN))
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        {
            const auto& names = choice->choices;
            for (int i = firstVisible; i < names.size(); ++i)
            {
                auto* b = buttons.add (new juce::TextButton (names[i]));
                b->setColour (juce::TextButton::buttonColourId,
                              juce::Colour (NorcoastLookAndFeel::kPanelBg));
                b->setColour (juce::TextButton::buttonOnColourId, accent);
                b->setColour (juce::TextButton::textColourOnId,
                              juce::Colour (NorcoastLookAndFeel::kBg));
                // Web-app style: inactive pill text is dim white (not
                // accent-tinted), so the active pill can light up
                // distinctly when toggled.
                b->setColour (juce::TextButton::textColourOffId,
                              juce::Colour (0xffd6dae6).withAlpha (0.55f));
                b->onClick = [this, i] { selectIndex (i); };
                addAndMakeVisible (b);
            }
        }
        startTimerHz (10);
    }

    ~ChoiceButtonRow() override { stopTimer(); }

    // Override the on-colour for a single visible button (e.g. so 4/4
    // and 6/8 can show different active hues for at-a-glance reading).
    void setButtonAccent (int visibleIndex, juce::Colour accent)
    {
        if (auto* b = buttons[visibleIndex])
        {
            b->setColour (juce::TextButton::buttonOnColourId, accent);
            // Off-state stays dim white; the accent only shows when
            // the pill is active.
            b->setColour (juce::TextButton::textColourOffId,
                          juce::Colour (0xffd6dae6).withAlpha (0.55f));
            b->repaint();
        }
    }

    void resized() override
    {
        if (buttons.isEmpty()) return;
        const int n  = buttons.size();
        const int rows = juce::jmin (numRows, n);
        const int cols = (n + rows - 1) / rows;
        const int w = getWidth()  / juce::jmax (1, cols);
        const int h = getHeight() / juce::jmax (1, rows);
        for (int i = 0; i < n; ++i)
        {
            const int r = i / cols;
            const int c = i % cols;
            buttons[i]->setBounds (c * w, r * h, w - 2, h - 2);
        }
    }

private:
    void timerCallback() override
    {
        const int idx = (int) std::round (apvts.getRawParameterValue (paramID)->load());
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i + firstVisible == idx, juce::dontSendNotification);
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
    int          numRows      = 1;
    int          firstVisible = 0;
    juce::OwnedArray<juce::TextButton> buttons;
};
