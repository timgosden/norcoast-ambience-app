#include "PluginEditor.h"

NorcoastAmbienceEditor::NorcoastAmbienceEditor (NorcoastAmbienceProcessor& p)
    : juce::AudioProcessorEditor (&p),
      owner    (p),
      keyboard (p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible (keyboard);
    keyboard.setLowestVisibleKey (48); // C3
    setSize (640, 380);
}

void NorcoastAmbienceEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour::fromRGB (0x07, 0x09, 0x0e), 0.0f, 0.0f,
                             juce::Colour::fromRGB (0x06, 0x08, 0x0d),
                             (float) getWidth(), (float) getHeight(), false);
    bg.addColour (0.4, juce::Colour::fromRGB (0x0b, 0x0f, 0x1a));
    g.setGradientFill (bg);
    g.fillAll();

    auto bounds = getLocalBounds().reduced (24);

    g.setColour (juce::Colour::fromRGBA (0xc4, 0x91, 0x5e, 0xcc));
    g.setFont (juce::FontOptions (20.0f).withStyle ("Light"));
    g.drawText ("NORCOAST AMBIENCE",
                bounds.removeFromTop (40),
                juce::Justification::centred);

    g.setColour (juce::Colour::fromRGBA (0xff, 0xff, 0xff, 0x55));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("plugin · v0.5 · phase 3a (Foundation + Pads)",
                getLocalBounds().removeFromBottom (24).reduced (8),
                juce::Justification::centred);
}

void NorcoastAmbienceEditor::resized()
{
    auto bounds = getLocalBounds();
    keyboard.setBounds (bounds.removeFromBottom (110).reduced (12, 12));
}
