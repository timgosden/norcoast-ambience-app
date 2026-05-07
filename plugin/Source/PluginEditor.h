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
    std::unique_ptr<ChoiceButtonRow> arpOctavesRow;
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

    // EQ panel is collapsed by default — matches the web app's "EQ ▾"
    // toggle. Click expands the four EQ knobs into row 2.
    juce::TextButton  eqToggleButton   { "EQ ▾" };
    bool              eqExpanded       = false;


    juce::ComboBox    presetBox;
    juce::TextButton  saveButton  { "Save" };
    juce::TextButton  loadButton  { "Load" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    void applyFactoryPreset (int idx);
    void savePresetToFile();
    void loadPresetFromFile();

    // Bottom-strip backplane: a single rounded panel that frames the
    // 8-knob FX row + 8-fader mixer row.
    juce::Rectangle<int> mixerPanelBounds;

    // ─── Mixer surface: 8 vertical faders across the bottom half ─────
    // Foundation, Pads 1, Pads 2, Texture, Arp, Movement (drums),
    // Low-Pass Filter, Master Vol — like a Korg nanoKONTROL strip.
    ParamKnob foundationVol, padsVol, padsVol2, textureVol;
    ParamKnob arpVol, drumVol, lpfFreq, masterVol;

    // Mute buttons for the 6 audio layers (LPF and Master never mute).
    juce::TextButton foundationMuteBtn { "M" };
    juce::TextButton padsMuteBtn       { "M" };
    juce::TextButton pads2MuteBtn      { "M" };
    juce::TextButton textureMuteBtn    { "M" };
    juce::TextButton arpMuteBtn        { "M" };
    juce::TextButton drumMuteBtn       { "M" };
    std::unique_ptr<ButtonAttach> foundationMuteAttach;
    std::unique_ptr<ButtonAttach> padsMuteAttach;
    std::unique_ptr<ButtonAttach> pads2MuteAttach;
    std::unique_ptr<ButtonAttach> textureMuteAttach;
    std::unique_ptr<ButtonAttach> arpMuteAttach;
    std::unique_ptr<ButtonAttach> drumMuteAttach;

    // ─── 8 FX mix knobs row above the faders ─────────────────────────
    // Reverb, Reverb Size, Shimmer, Chorus, Delay, Delay Feedback,
    // Width, Drive — all "amount" controls.
    ParamKnob reverbMix, reverbSize, shimmerVol, chorusMix;
    ParamKnob delayMix, delayFb, widthMod, satAmt;

    // ─── Other controls (top half) ───────────────────────────────────
    ParamKnob evolveRate;
    ParamKnob delayTimeMs, delayTone, reverbMod;
    ParamKnob hpfFreq;
    ParamKnob eqLow, eqLoMid, eqHiMid, eqHigh;
    ParamKnob arpRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorcoastAmbienceEditor)
};
