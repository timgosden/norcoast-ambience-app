#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#endif

NorcoastAmbienceEditor::NorcoastAmbienceEditor (NorcoastAmbienceProcessor& p)
    : juce::AudioProcessorEditor (&p),
      owner    (p),
      keyboard (p.getKeyboardState())
{
    addAndMakeVisible (keyboard);
    keyboard.setLowestVisibleKey (48); // C3

    addAndMakeVisible (latchButton);
    latchButton.setClickingTogglesState (true);
    latchButton.onClick = [this]
    {
        keyboard.setLatched (latchButton.getToggleState());
    };

    addAndMakeVisible (allOffButton);
    allOffButton.onClick = [this]
    {
        owner.getKeyboardState().allNotesOff (1);
    };

    // Standalone-only — host plugins route MIDI via the host itself.
   #if JucePlugin_Build_Standalone
    addAndMakeVisible (settingsButton);
    settingsButton.onClick = []
    {
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            holder->showAudioSettingsDialog();
    };
   #endif

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
    g.drawText ("plugin · v0.10 · phase 3f (+ MIDI controls)",
                getLocalBounds().removeFromBottom (24).reduced (8),
                juce::Justification::centred);
}

void NorcoastAmbienceEditor::resized()
{
    auto bounds = getLocalBounds();

    // Bottom strip: keyboard
    auto kbStrip = bounds.removeFromBottom (110).reduced (12, 12);
    keyboard.setBounds (kbStrip);

    // Just above the keyboard: control buttons row.
    auto buttons = bounds.removeFromBottom (38).reduced (12, 4);
    const int buttonW = 110;
    latchButton.setBounds    (buttons.removeFromLeft  (buttonW));
    buttons.removeFromLeft (8);
    allOffButton.setBounds   (buttons.removeFromLeft  (buttonW));

   #if JucePlugin_Build_Standalone
    settingsButton.setBounds (buttons.removeFromRight (140));
   #endif
}
