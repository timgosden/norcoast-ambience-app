#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "LatchableKeyboard.h"
#include "NorcoastLookAndFeel.h"

class NorcoastAmbienceEditor : public juce::AudioProcessorEditor
{
public:
    explicit NorcoastAmbienceEditor (NorcoastAmbienceProcessor&);
    ~NorcoastAmbienceEditor() override;

    void paint    (juce::Graphics&) override;
    void resized()                  override;

private:
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // One rotary knob with a top label and APVTS attachment.
    struct ParamKnob
    {
        juce::Label  label;
        juce::Slider knob;
        std::unique_ptr<SliderAttach> attach;
    };

    void setupKnob (ParamKnob& k, const juce::String& displayName,
                    const juce::String& paramID,
                    const juce::String& suffix = {});

    NorcoastLookAndFeel laf;

    NorcoastAmbienceProcessor& owner;
    LatchableKeyboard keyboard;
    juce::TextButton  latchButton  { "Latch" };
    juce::TextButton  allOffButton { "All Off" };

    juce::TextButton  subOctButton    { "Sub Oct" };
    juce::TextButton  textureOctButton { "Tex +Oct" };
    std::unique_ptr<ButtonAttach> subOctAttach;
    std::unique_ptr<ButtonAttach> textureOctAttach;

    juce::ComboBox    presetBox;
    juce::TextButton  saveButton  { "Save" };
    juce::TextButton  loadButton  { "Load" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    void applyFactoryPreset (int idx);
    void savePresetToFile();
    void loadPresetFromFile();

    // Six logical sections, drawn as rounded-rect panels with a header.
    // Each holds one or more knobs.
    struct Section
    {
        juce::String title;
        juce::Colour accent;
        juce::Rectangle<int> bounds;
        std::vector<ParamKnob*> knobs;
    };
    std::array<Section, 8> sections;

    // Knobs (allocated as members so their lifetime matches the editor)
    ParamKnob foundationVol, padsVol, textureVol;
    ParamKnob chorusMix, delayMix, delayFb, delayTimeMs, delayTone;
    ParamKnob reverbMix, reverbSize, reverbMod, shimmerVol;
    ParamKnob hpfFreq, lpfFreq;
    ParamKnob eqLow, eqLoMid, eqHiMid, eqHigh;
    ParamKnob widthMod, satAmt, masterVol;

    ParamKnob arpVol, arpRate, arpOctaves, arpVoice;
    ParamKnob drumVol, drumPattern;
    ParamKnob velocitySens, pitchBendRange;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorcoastAmbienceEditor)
};
