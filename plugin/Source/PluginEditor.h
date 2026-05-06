#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "NorcoastLookAndFeel.h"
#include "ChordStateHeader.h"
#include "ChoiceButtonRow.h"
#include "BitmaskPillRow.h"
#include "StepSequencerGrid.h"

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

    // Invisible MidiKeyboardComponent — exists only to capture qwerty
    // keys via JUCE's tested key-press → MIDI mapping. The user never
    // sees it.
    juce::MidiKeyboardComponent qwertyKeyboard;

    // Big "C · Maj7th" centred header — visual heartbeat of the plugin.
    ChordStateHeader chordHeader;

    // Stop pill — 1 s master fade-out / fade-in.
    juce::TextButton stopButton { "Stop" };

    // Choice button rows — replace knobs for discrete pickers.
    std::unique_ptr<ChoiceButtonRow> arpVoiceRow;
    std::unique_ptr<ChoiceButtonRow> drumPatternRow;
    std::unique_ptr<BitmaskPillRow>  chordPoolRow;     // EVOLVE chord-pool toggles
    std::unique_ptr<ChoiceButtonRow> rootKeyRow;       // bottom 12-key grid
    std::unique_ptr<BitmaskPillRow>  customDegreesRow; // custom-chord degree pills

    std::unique_ptr<StepSequencerGrid> stepSequencer;  // 16-step drum grid


    juce::TextButton  subOctButton     { "Sub Oct" };
    juce::TextButton  textureOctButton { "Tex +Oct" };
    juce::TextButton  evolveButton     { "Evolve" };
    juce::TextButton  droneButton      { "Drone" };
    std::unique_ptr<ButtonAttach> subOctAttach;
    std::unique_ptr<ButtonAttach> textureOctAttach;
    std::unique_ptr<ButtonAttach> evolveAttach;
    std::unique_ptr<ButtonAttach> droneAttach;


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
    std::array<Section, 10> sections;

    // Knobs (allocated as members so their lifetime matches the editor)
    ParamKnob foundationVol, padsVol, textureVol;
    ParamKnob evolveRate;
    ParamKnob chorusMix, delayMix, delayFb, delayTimeMs, delayTone;
    ParamKnob reverbMix, reverbSize, reverbMod, shimmerVol;
    ParamKnob hpfFreq, lpfFreq;
    ParamKnob eqLow, eqLoMid, eqHiMid, eqHigh;
    ParamKnob widthMod, satAmt, masterVol;

    ParamKnob arpVol, arpRate, arpOctaves;
    ParamKnob drumVol;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorcoastAmbienceEditor)
};
