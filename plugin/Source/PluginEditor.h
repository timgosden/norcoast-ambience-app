#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "LatchableKeyboard.h"

class NorcoastAmbienceEditor : public juce::AudioProcessorEditor
{
public:
    explicit NorcoastAmbienceEditor (NorcoastAmbienceProcessor&);
    ~NorcoastAmbienceEditor() override = default;

    void paint    (juce::Graphics&) override;
    void resized()                  override;

private:
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;

    // Bundled label + slider + APVTS attachment.
    struct ParamControl
    {
        juce::Label  label;
        juce::Slider slider;
        std::unique_ptr<SliderAttach> attach;
    };

    void addControl (ParamControl& pc, const juce::String& displayName,
                     const juce::String& paramID);

    NorcoastAmbienceProcessor& owner;
    LatchableKeyboard keyboard;
    juce::TextButton  latchButton    { "Latch" };
    juce::TextButton  allOffButton   { "All Off" };
    juce::TextButton  settingsButton { "MIDI / Audio…" };

    // Section headers
    juce::Label layersHeader, fxHeader, eqHeader, masterHeader;

    // Layer column
    ParamControl foundationVol, padsVol;

    // FX column
    ParamControl chorusMix;
    ParamControl delayMix, delayFb, delayTimeMs, delayTone;
    ParamControl reverbMix, reverbSize, reverbMod;

    // EQ column
    ParamControl eqLow, eqLoMid, eqHiMid, eqHigh;

    // Master column (filters first, then output)
    ParamControl hpfFreq, lpfFreq, shimmerVol, widthMod, satAmt, masterVol;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorcoastAmbienceEditor)
};
