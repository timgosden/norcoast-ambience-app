#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "NorcoastLookAndFeel.h"
#include "ChordStateHeader.h"
#include "ChoiceButtonRow.h"
#include "BitmaskPillRow.h"
#include "StepSequencerGrid.h"
#include "EqCurveDisplay.h"

class NorcoastAmbienceEditor : public juce::AudioProcessorEditor,
                               private juce::Timer,
                               private juce::Slider::Listener
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

    // Hover-tooltip support — every knob / button / fader gets a
    // setTooltip() call so the user can discover what each does.
    juce::TooltipWindow tooltipWindow { this, 600 };

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
    juce::TextButton  deleteButton{ "Delete" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    void applyFactoryPreset (int idx);
    void savePresetToFile();
    void loadPresetFromFile();
    void confirmDeleteSelectedUserPreset();
    std::unique_ptr<juce::AlertWindow> deletePromptWindow;

    // ─── User-preset storage ────────────────────────────────────────
    // User presets live as .ncpre XML files in a known directory. The
    // dropdown shows factory presets first, then a "— User —" header,
    // then user presets discovered on disk; the IDs above kUserItemBase
    // identify a user-preset file by its index in userPresetFiles.
    static constexpr int kUserItemBase = 1000;
    juce::Array<juce::File> userPresetFiles;
    juce::File getUserPresetDir() const;
    void rebuildPresetMenu (int selectFactoryIdx = -1,
                            int selectUserIdx    = -1);
    void promptSaveUserPreset();
    std::unique_ptr<juce::AlertWindow> savePromptWindow;

    // Bottom-strip backplane: a single rounded panel that frames the
    // 8-knob FX row + 8-fader mixer row.
    juce::Rectangle<int> mixerPanelBounds;

    // Smoothed level-meter values for the 6 audio layer faders. Audio
    // thread writes processor.layerLevels[i]; the editor's timer reads
    // them, applies a 0.85-decay envelope, and the paint() routine
    // draws a thin orange bar overlay inside each fader column.
    std::array<float, 6> meterLevels { 0, 0, 0, 0, 0, 0 };
    void timerCallback() override;

    // Live Hz/kHz readout above the LPF fader column — fills the slot
    // where the other layer columns show their octave toggle. Updated
    // from the LPF slider value in timerCallback().
    juce::Label lpfHzLabel;

    // Visual Stop fade multiplier. Eases toward 0 while the Stop pill
    // is engaged and back toward 1 when released, using the same
    // fadeTime as the audio. Drives the dim overlay over each layer
    // fader so you see the playable headroom drop with the audio.
    float  visualStopMult        = 1.0f;
    double lastVisualStopUpdateMs = 0.0;

    // Cached last home-root index; used to relabel the Custom Chord
    // pills (1·C, 2·D, …) only when the root actually changes.
    int lastHomeRootForLabels = -1;

    // Slider drag tracking — when a fader is being dragged, paint a
    // floating "-X.X dB" readout above its thumb (same UX as the EQ
    // band-node drag readout).
    juce::Slider* activeFader = nullptr;
    void sliderDragStarted (juce::Slider* s) override;
    void sliderDragEnded   (juce::Slider* s) override;
    void sliderValueChanged (juce::Slider* s) override;

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
    ParamKnob fadeTime, keyXfade;                  // Stop fade + key change crossfade
    ParamKnob eqLow, eqLoMid, eqHiMid, eqHigh;     // host-only; UI uses eqCurve
    std::unique_ptr<EqCurveDisplay> eqCurve;

    // ─── Advanced panel toggle ───────────────────────────────────────
    juce::TextButton advButton { "Adv" };
    bool             advExpanded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorcoastAmbienceEditor)
};
