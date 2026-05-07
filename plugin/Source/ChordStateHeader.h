#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "NorcoastLookAndFeel.h"
#include "ChordEvolver.h"

// Big "C · Maj7th" display — the web app's visual heartbeat. The root
// note is huge and accent-orange; the chord-type name is smaller and
// dim-white next to it. Polled at 15 Hz off the APVTS atomic store
// (no per-block redraw cost).
class ChordStateHeader : public juce::Component, private juce::Timer
{
public:
    explicit ChordStateHeader (juce::AudioProcessorValueTreeState& s)
        : apvts (s)
    {
        startTimerHz (15);
    }

    ~ChordStateHeader() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const int rootIdx  = juce::jlimit (0, 11,
                              (int) apvts.getRawParameterValue (ParamID::homeRoot)->load());
        const int chordIdx = juce::jlimit (0, (int) ChordEvolver::NumTypes - 1,
                              (int) apvts.getRawParameterValue (ParamID::chordType)->load());

        // Match the homeRoot param's choice list (Parameters.h) so the
        // big root display in the header uses the same spelling as the
        // root-key picker below it.
        static const juce::StringArray rootNames
            { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        const auto chordNames = ChordEvolver::getChordNames();

        const auto bounds = getLocalBounds().toFloat();
        const float h = bounds.getHeight();

        const auto rootText  = rootNames[rootIdx];
        const auto chordText = chordNames[chordIdx];

        const auto rootFont  = juce::FontOptions (h * 0.85f).withStyle ("Light");
        const auto chordFont = juce::FontOptions (h * 0.45f).withStyle ("Light");
        const auto dotFont   = juce::FontOptions (h * 0.40f);

        const float rootW  = juce::Font (rootFont) .getStringWidthFloat (rootText);
        const float chordW = juce::Font (chordFont).getStringWidthFloat (chordText);
        const float dotW   = juce::Font (dotFont)  .getStringWidthFloat (" \xc2\xb7 ");

        const float total  = rootW + dotW + chordW;
        float x = bounds.getCentreX() - total * 0.5f;

        g.setFont (rootFont);
        g.setColour (juce::Colour (NorcoastLookAndFeel::kAccent));
        g.drawText (rootText, juce::Rectangle<float> (x, 0, rootW, h),
                    juce::Justification::centredLeft);
        x += rootW;

        g.setFont (dotFont);
        g.setColour (juce::Colour (0x33ffffff));
        g.drawText (juce::String::fromUTF8 (" \xc2\xb7 "),
                    juce::Rectangle<float> (x, 0, dotW, h),
                    juce::Justification::centred);
        x += dotW;

        g.setFont (chordFont);
        g.setColour (juce::Colour (0xd1ffffff));
        g.drawText (chordText, juce::Rectangle<float> (x, 0, chordW, h),
                    juce::Justification::centredLeft);
    }

private:
    void timerCallback() override { repaint(); }

    juce::AudioProcessorValueTreeState& apvts;
};
