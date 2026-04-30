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
    NorcoastAmbienceProcessor& owner;
    LatchableKeyboard keyboard;
    juce::TextButton  latchButton    { "Latch" };
    juce::TextButton  allOffButton   { "All Off" };
    juce::TextButton  settingsButton { "MIDI / Audio…" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorcoastAmbienceEditor)
};
