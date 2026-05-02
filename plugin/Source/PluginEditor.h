#pragma once

#include "PluginProcessor.h"

class NorcoastAmbienceEditor : public juce::AudioProcessorEditor
{
public:
    explicit NorcoastAmbienceEditor (NorcoastAmbienceProcessor&);
    ~NorcoastAmbienceEditor() override = default;

    void paint    (juce::Graphics&) override;
    void resized()                  override;

private:
    NorcoastAmbienceProcessor& processor;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorcoastAmbienceEditor)
};
