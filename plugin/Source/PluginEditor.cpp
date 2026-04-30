#include "PluginEditor.h"
#include "Parameters.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#endif

namespace
{
    constexpr auto kAccent = 0xffc4915e; // standalone's "warm orange"
    constexpr auto kBg     = 0xff07090e;
    constexpr auto kBgMid  = 0xff0b0f1a;
    constexpr auto kBgEdge = 0xff06080d;
    constexpr auto kRowBg  = 0xff111620;
    constexpr auto kText   = 0xffe6e1d8;
    constexpr auto kTextMute = 0x99ffffff;

    void styleHeader (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, juce::Colour (kAccent));
        l.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
        l.setJustificationType (juce::Justification::centredLeft);
    }
}

void NorcoastAmbienceEditor::addControl (ParamControl& pc, const juce::String& name,
                                         const juce::String& paramID)
{
    pc.label.setText (name, juce::dontSendNotification);
    pc.label.setColour (juce::Label::textColourId, juce::Colour (kText));
    pc.label.setFont (juce::FontOptions (11.0f));
    pc.label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (pc.label);

    pc.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    pc.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    pc.slider.setColour (juce::Slider::trackColourId,        juce::Colour (kAccent));
    pc.slider.setColour (juce::Slider::backgroundColourId,   juce::Colour (kRowBg));
    pc.slider.setColour (juce::Slider::thumbColourId,        juce::Colour (kAccent).brighter (0.2f));
    pc.slider.setColour (juce::Slider::textBoxTextColourId,  juce::Colour (kTextMute));
    pc.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    pc.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (pc.slider);

    pc.attach = std::make_unique<SliderAttach> (owner.getAPVTS(), paramID, pc.slider);
}

NorcoastAmbienceEditor::NorcoastAmbienceEditor (NorcoastAmbienceProcessor& p)
    : juce::AudioProcessorEditor (&p),
      owner    (p),
      keyboard (p.getKeyboardState())
{
    addAndMakeVisible (keyboard);
    keyboard.setLowestVisibleKey (48);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId,
                        juce::Colour::fromRGB (0xc4, 0xc0, 0xb6));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId,
                        juce::Colour (kBgEdge));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
                        juce::Colour (kAccent).withAlpha (0.55f));

    addAndMakeVisible (latchButton);
    latchButton.setClickingTogglesState (true);
    latchButton.onClick = [this] { keyboard.setLatched (latchButton.getToggleState()); };

    addAndMakeVisible (allOffButton);
    allOffButton.onClick = [this] { owner.getKeyboardState().allNotesOff (1); };

   #if JucePlugin_Build_Standalone
    addAndMakeVisible (settingsButton);
    settingsButton.onClick = []
    {
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            holder->showAudioSettingsDialog();
    };
   #endif

    styleHeader (layersHeader, "LAYERS");
    styleHeader (fxHeader,     "FX");
    styleHeader (eqHeader,     "EQ");
    styleHeader (masterHeader, "MASTER");
    addAndMakeVisible (layersHeader);
    addAndMakeVisible (fxHeader);
    addAndMakeVisible (eqHeader);
    addAndMakeVisible (masterHeader);

    addControl (foundationVol, "Foundation",     ParamID::foundationVol);
    addControl (padsVol,       "Pads",           ParamID::padsVol);

    addControl (chorusMix,     "Chorus",         ParamID::chorusMix);
    addControl (delayMix,      "Delay Mix",      ParamID::delayMix);
    addControl (delayFb,       "Delay Feedback", ParamID::delayFb);
    addControl (delayTimeMs,   "Delay Time",     ParamID::delayTimeMs);
    addControl (delayTone,     "Delay Tone",     ParamID::delayTone);
    addControl (reverbMix,     "Reverb Mix",     ParamID::reverbMix);
    addControl (reverbSize,    "Reverb Size",    ParamID::reverbSize);
    addControl (reverbMod,     "Reverb Mod",     ParamID::reverbMod);

    addControl (eqLow,         "Low (100)",      ParamID::eqLow);
    addControl (eqLoMid,       "Lo-Mid (350)",   ParamID::eqLoMid);
    addControl (eqHiMid,       "Hi-Mid (2.5k)",  ParamID::eqHiMid);
    addControl (eqHigh,        "High (8k)",      ParamID::eqHigh);

    addControl (hpfFreq,       "High Pass",      ParamID::hpfFreq);
    addControl (lpfFreq,       "Low Pass",       ParamID::lpfFreq);
    addControl (shimmerVol,    "Shimmer",        ParamID::shimmerVol);
    addControl (widthMod,      "Width LFO",      ParamID::widthMod);
    addControl (satAmt,        "Saturation",     ParamID::satAmt);
    addControl (masterVol,     "Master",         ParamID::masterVol);

    setSize (820, 560);
}

void NorcoastAmbienceEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (kBg), 0.0f, 0.0f,
                             juce::Colour (kBgEdge),
                             (float) getWidth(), (float) getHeight(), false);
    bg.addColour (0.4, juce::Colour (kBgMid));
    g.setGradientFill (bg);
    g.fillAll();

    auto bounds = getLocalBounds().reduced (24);

    g.setColour (juce::Colour (kAccent).withAlpha (0.85f));
    g.setFont (juce::FontOptions (20.0f).withStyle ("Light"));
    g.drawText ("NORCOAST AMBIENCE",
                bounds.removeFromTop (32),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0x33ffffff));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("plugin · v1.2 · phase 6 (+ master HPF/LPF)",
                getLocalBounds().removeFromBottom (24).reduced (12, 4),
                juce::Justification::centredRight);
}

void NorcoastAmbienceEditor::resized()
{
    auto bounds = getLocalBounds().reduced (24);

    // Top bar — title is drawn in paint(); reserve it here, then place the
    // settings button on the right.
    auto top = bounds.removeFromTop (32);
   #if JucePlugin_Build_Standalone
    settingsButton.setBounds (top.removeFromRight (140).reduced (0, 4));
   #endif

    bounds.removeFromTop (12);

    // Slider grid: 4 columns side by side.
    auto grid = bounds.removeFromTop (290);

    const int colW    = grid.getWidth() / 4;
    const int rowH    = 36;
    const int headerH = 20;

    auto layoutColumn = [&] (juce::Rectangle<int> col, juce::Label& header,
                             std::initializer_list<ParamControl*> controls)
    {
        col.reduce (8, 0);
        header.setBounds (col.removeFromTop (headerH));
        col.removeFromTop (4);
        for (auto* c : controls)
        {
            auto row = col.removeFromTop (rowH);
            c->label.setBounds  (row.removeFromTop (14));
            c->slider.setBounds (row.removeFromTop (20));
        }
    };

    layoutColumn (grid.removeFromLeft (colW), layersHeader,
                  { &foundationVol, &padsVol });
    layoutColumn (grid.removeFromLeft (colW), fxHeader,
                  { &chorusMix, &delayMix, &delayFb, &delayTimeMs,
                    &delayTone, &reverbMix, &reverbSize, &reverbMod });
    layoutColumn (grid.removeFromLeft (colW), eqHeader,
                  { &eqLow, &eqLoMid, &eqHiMid, &eqHigh });
    layoutColumn (grid,                       masterHeader,
                  { &hpfFreq, &lpfFreq, &shimmerVol, &widthMod, &satAmt, &masterVol });

    // Latch + All Off buttons row
    auto buttons = bounds.removeFromBottom (160).removeFromTop (32);
    const int buttonW = 110;
    latchButton.setBounds  (buttons.removeFromLeft (buttonW));
    buttons.removeFromLeft (8);
    allOffButton.setBounds (buttons.removeFromLeft (buttonW));

    // Keyboard
    auto kbStrip = getLocalBounds().removeFromBottom (124).reduced (24, 12);
    keyboard.setBounds (kbStrip);
}
