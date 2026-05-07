#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "NorcoastLookAndFeel.h"
#include "ChordStateHeader.h"
#include "ChoiceButtonRow.h"
#include "BitmaskPillRow.h"
#include "StepSequencerGrid.h"
#include "EqCurveDisplay.h"

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
    std::unique_ptr<ChoiceButtonRow> arpRateRow;        // ⏯ rate as buttons not knob
    std::unique_ptr<ChoiceButtonRow> evolveBarsRow;     // bars as buttons not knob
    std::unique_ptr<ChoiceButtonRow> drumPatternRow;
    std::unique_ptr<BitmaskPillRow>  chordPoolRow;     // EVOLVE chord-pool toggles
    std::unique_ptr<ChoiceButtonRow> rootKeyRow;       // bottom 12-key grid
    std::unique_ptr<BitmaskPillRow>  customDegreesRow; // custom-chord degree pills

    std::unique_ptr<StepSequencerGrid> stepSequencer;  // 16-step drum grid


    // Per-layer octave toggles — one button each, sit above the
    // matching fader column in the mixer for one-glance control.
    juce::TextButton  subOctButton      { "-Oct" };     // Foundation sub
    juce::TextButton  padsSubOctButton  { "-Oct" };     // Pads 1 sub
    juce::TextButton  pads2SubOctButton { "-Oct" };     // Pads 2 sub
    juce::TextButton  textureOctButton  { "+Oct" };     // Texture super
    juce::TextButton  evolveButton      { "Evolve" };   // hidden; default on
    juce::TextButton  droneButton       { "Drone" };    // hidden; default on
    std::unique_ptr<ButtonAttach> subOctAttach;
    std::unique_ptr<ButtonAttach> padsSubOctAttach;
    std::unique_ptr<ButtonAttach> pads2SubOctAttach;
    std::unique_ptr<ButtonAttach> textureOctAttach;
    std::unique_ptr<ButtonAttach> evolveAttach;
    std::unique_ptr<ButtonAttach> droneAttach;

    // Custom Chord / Step Sequencer panels are collapsed by default.
    // Each toggle button flips the panel and triggers a re-layout.
    juce::TextButton  customChordToggleButton { "Custom Chord +" };
    juce::TextButton  sequencerToggleButton   { "Sequencer +" };
    bool              customChordExpanded = false;
    bool              sequencerExpanded   = false;

    // 4/4 vs 6/8 selector — important enough to live next to the drum
    // pattern pills so the time-sig is always visible.
    std::unique_ptr<ChoiceButtonRow> timeSigRow;

    // Manual BPM. The plugin reads bpm from the host playhead when one
    // is provided; otherwise it falls back to this user-controllable
    // value (default 70). The Slider exposes ±/scroll editing.
    juce::Label  bpmLabel;
    juce::Slider bpmSlider;
    std::unique_ptr<SliderAttach> bpmAttach;


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

    // Top-half coloured-strip backgrounds — drawn in paint() to give
    // each control row its own section accent (matches the web app's
    // colour-per-section design language).
    juce::Rectangle<int> evolveStripBounds;
    juce::Rectangle<int> drumsStripBounds;
    juce::Rectangle<int> arpStripBounds;

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

    // ─── FX mix knobs row above the faders ───────────────────────────
    // Reverb · Shimmer · Chorus · Delay · Width · Drive · HPF.
    // Slot 8 (above Master) is intentionally empty.
    ParamKnob reverbMix, shimmerVol, chorusMix;
    ParamKnob delayMix,  widthMod,   satAmt;

    // ─── Other surfaced controls ─────────────────────────────────────
    // HPF lives in the mixer above the LPF fader.
    // The remaining knobs are surfaced only on the Advanced panel
    // (toggled by advButton in the title bar). When Advanced is OFF
    // these are invisible; their params still affect audio.
    ParamKnob hpfFreq;
    ParamKnob reverbSize, reverbMod;
    ParamKnob delayFb, delayTimeMs, delayTone;
    ParamKnob eqLow, eqLoMid, eqHiMid, eqHigh;     // host-only; UI uses eqCurve
    std::unique_ptr<EqCurveDisplay> eqCurve;

    // ─── Advanced panel toggle ───────────────────────────────────────
    juce::TextButton advButton { "Adv" };
    bool             advExpanded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorcoastAmbienceEditor)
};
