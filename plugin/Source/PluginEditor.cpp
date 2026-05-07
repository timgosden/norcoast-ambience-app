#include "PluginEditor.h"
#include "Parameters.h"
#include "Presets.h"
#include "ChordEvolver.h"

namespace
{
    constexpr int kKnobLabelH     = 16;
    constexpr int kSectionPadX    = 12;
    constexpr int kSectionPadY    = 8;
    constexpr int kSectionHeaderH = 20;

    // Per-section accent colours pulled from the standalone's slider strips.
    // Section accent palette — mirrors the web app's per-layer colours
    // (public/index.html). Each section gets its own hue so the page
    // reads as a colour-coded heat map at a glance.
    const juce::Colour kColLayers   { 0xff5eb88a }; // green   — Foundation
    const juce::Colour kColEvolve   { 0xffc4915e }; // amber   — chord evolution
    const juce::Colour kColChorus   { 0xffc4b8e8 }; // lavender — chorus / modulation
    const juce::Colour kColDelay    { 0xff5eb88a }; // green   — delay
    const juce::Colour kColReverbFx { 0xff6a9fd8 }; // blue    — reverb
    const juce::Colour kColFilter   { 0xffd46b8a }; // pink    — filter
    const juce::Colour kColEq       { 0xffb07acc }; // purple  — EQ
    const juce::Colour kColArp      { 0xff9b7fd4 }; // purple  — arpeggiator
    const juce::Colour kColDrums    { 0xffe8a45e }; // orange  — movement / drums
    const juce::Colour kColMaster   { 0xffc4915e }; // amber   — master volume

    void styleKnobLabel (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId,
                     juce::Colour (NorcoastLookAndFeel::kTextDim));
        // Scale text down (rather than truncate with "...") for long labels
        // like "Foundation" / "Saturation" in narrow knob slots.
        l.setMinimumHorizontalScale (0.6f);
    }
}

void NorcoastAmbienceEditor::setupKnob (ParamKnob& k, const juce::String& displayName,
                                        const juce::String& paramID,
                                        const juce::String& suffix)
{
    styleKnobLabel (k.label, displayName);
    addAndMakeVisible (k.label);

    k.knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f,
                                true);
    k.knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
    k.knob.setLookAndFeel (&laf);
    addAndMakeVisible (k.knob);

    k.attach = std::make_unique<SliderAttach> (owner.getAPVTS(), paramID, k.knob);

    // Default value-display formatting depends on the suffix:
    //   "%"   → percent of 0..1
    //   " dB" → one decimal + dB
    //   ""    → integer percent of 0..1 (default for unitless params)
    //   anything else → number + suffix, no decimals
    if (suffix == "%" || suffix.isEmpty())
    {
        k.knob.textFromValueFunction = [] (double v) -> juce::String
        {
            return juce::String ((int) std::round (v * 100.0)) + "%";
        };
    }
    else if (suffix == " dB")
    {
        k.knob.textFromValueFunction = [] (double v) -> juce::String
        {
            return juce::String (v, 1) + " dB";
        };
    }
    else
    {
        k.knob.setTextValueSuffix (suffix);
    }
    k.knob.updateText();

    // Double-click resets to the parameter default.
    if (auto* p = owner.getAPVTS().getParameter (paramID))
    {
        const float def = p->getNormalisableRange()
                            .convertFrom0to1 (p->getDefaultValue());
        k.knob.setDoubleClickReturnValue (true, def);
    }
}

NorcoastAmbienceEditor::NorcoastAmbienceEditor (NorcoastAmbienceProcessor& p)
    : juce::AudioProcessorEditor (&p),
      owner (p),
      qwertyKeyboard (p.getKeyboardState(),
                      juce::MidiKeyboardComponent::horizontalKeyboard),
      chordHeader (p.getAPVTS())
{
    setLookAndFeel (&laf);

    addAndMakeVisible (qwertyKeyboard);
    qwertyKeyboard.setKeyPressBaseOctave (4);
    qwertyKeyboard.setWantsKeyboardFocus (true);
    qwertyKeyboard.setVelocity (0.7f, false);

    addAndMakeVisible (chordHeader);

    addAndMakeVisible (stopButton);
    stopButton.setClickingTogglesState (true);
    stopButton.setColour (juce::TextButton::buttonColourId,
                          juce::Colour (NorcoastLookAndFeel::kPanelBg));
    stopButton.setColour (juce::TextButton::buttonOnColourId,
                          juce::Colour (0xffd46b8a));      // standalone "stop" pink
    stopButton.setColour (juce::TextButton::textColourOnId,
                          juce::Colour (NorcoastLookAndFeel::kBg));
    stopButton.setColour (juce::TextButton::textColourOffId,
                          juce::Colour (0xffd46b8a).withAlpha (0.7f));
    stopButton.onClick = [this]
    {
        owner.setStopped (stopButton.getToggleState());
    };

    // Per-layer octave toggles. setupOctToggle wires up label + APVTS
    // attachment + accent colour in one call so all five rows are
    // configured uniformly.
    auto setupOctToggle = [this] (juce::TextButton& btn,
                                  const juce::String& paramID,
                                  std::unique_ptr<ButtonAttach>& attach,
                                  juce::Colour accent)
    {
        addAndMakeVisible (btn);
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::buttonColourId,
                       juce::Colour (NorcoastLookAndFeel::kPanelBg));
        btn.setColour (juce::TextButton::buttonOnColourId, accent);
        btn.setColour (juce::TextButton::textColourOnId,
                       juce::Colour (NorcoastLookAndFeel::kBg));
        attach = std::make_unique<ButtonAttach> (
            owner.getAPVTS(), paramID, btn);
    };
    setupOctToggle (subOctButton,      ParamID::foundationSubOct,
                    subOctAttach,      juce::Colour (0xff5eb88a));   // foundation green
    setupOctToggle (padsSubOctButton,  ParamID::padsSubOct,
                    padsSubOctAttach,  juce::Colour (0xffc4915e));   // pads amber
    setupOctToggle (pads2SubOctButton, ParamID::pads2SubOct,
                    pads2SubOctAttach, juce::Colour (0xffe0d2a8));   // pads 2 cream
    setupOctToggle (textureOctButton,  ParamID::textureOctUp,
                    textureOctAttach,  juce::Colour (0xffa08060));   // texture tan

    addAndMakeVisible (evolveButton);
    evolveButton.setClickingTogglesState (true);
    evolveAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::evolveOn, evolveButton);

    addAndMakeVisible (droneButton);
    droneButton.setClickingTogglesState (true);
    droneAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::droneOn, droneButton);

    // ── Collapsible-panel toggle buttons ────────────────────────────────
    // Custom-chord pills + drum step sequencer are collapsed by default;
    // each toggle button flips a flag, sets the visible label, and
    // triggers a re-layout. (EQ panel was removed; eq* params still
    // exist for preset compat but have no UI surface.)
    auto setupToggle = [this] (juce::TextButton& btn,
                               const juce::String& labelOn,
                               const juce::String& labelOff,
                               juce::Colour onColour,
                               bool* flagRefPtr)
    {
        addAndMakeVisible (btn);
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::buttonColourId,
                       juce::Colour (NorcoastLookAndFeel::kPanelBg));
        btn.setColour (juce::TextButton::buttonOnColourId, onColour.withAlpha (0.4f));
        btn.onClick = [this, &btn, labelOn, labelOff, flagRefPtr]
        {
            const bool open = btn.getToggleState();
            *flagRefPtr = open;
            btn.setButtonText (open ? labelOn : labelOff);
            resized();
            repaint();
        };
    };

    setupToggle (customChordToggleButton,
                 "Custom Chord -", "Custom Chord +",
                 juce::Colour (0xffc4915e),     // amber — chord-evolve accent
                 &customChordExpanded);

    setupToggle (sequencerToggleButton,
                 "Sequencer -", "Sequencer +",
                 juce::Colour (0xffe8a45e),     // orange — movement accent
                 &sequencerExpanded);

    // ── Preset bar ──────────────────────────────────────────────────────
    addAndMakeVisible (presetBox);
    presetBox.setColour (juce::ComboBox::backgroundColourId,
                         juce::Colour (NorcoastLookAndFeel::kPanelBg));
    presetBox.setColour (juce::ComboBox::textColourId,
                         juce::Colour (NorcoastLookAndFeel::kAccent));
    presetBox.setColour (juce::ComboBox::arrowColourId,
                         juce::Colour (NorcoastLookAndFeel::kAccent));
    int id = 1;
    for (const auto& preset : Presets::factory())
        presetBox.addItem (preset.name, id++);
    presetBox.setText ("Preset", juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0)
            owner.setCurrentProgram (idx);
    };

    addAndMakeVisible (saveButton);
    saveButton.onClick = [this] { savePresetToFile(); };
    addAndMakeVisible (loadButton);
    loadButton.onClick = [this] { loadPresetFromFile(); };

    setupKnob (foundationVol, "Foundation",   ParamID::foundationVol);
    setupKnob (padsVol,       "Anchor",       ParamID::padsVol);
    setupKnob (textureVol,    "Texture",      ParamID::textureVol);

    // EVOLVE chord pool — pills toggle which chord types the auto-evolve
    // cycle picks from. Default = all six on.
    chordPoolRow = std::make_unique<BitmaskPillRow> (
        owner.getAPVTS(), ParamID::enabledChordsMask,
        juce::StringArray { "5th", "Sus2", "Sus4", "Maj7", "9th", "Cust" },
        juce::Colour (NorcoastLookAndFeel::kAccent));
    addAndMakeVisible (*chordPoolRow);

    // Custom-chord degree pills — bit 0 (root) always on, 1..6 toggle the
    // 2nd…7th of the major scale.
    customDegreesRow = std::make_unique<BitmaskPillRow> (
        owner.getAPVTS(), ParamID::customChordMask,
        juce::StringArray { "1", "2", "3", "4", "5", "6", "7" },
        juce::Colour (NorcoastLookAndFeel::kAccent),
        /* alwaysOnBit = */ 0);
    addAndMakeVisible (*customDegreesRow);

    // 12-key root grid — 3 rows of 4 (C/Db/D/Eb · E/F/Gb/G · Ab/A/Bb/B).
    rootKeyRow = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::homeRoot,
        juce::Colour (NorcoastLookAndFeel::kAccent),
        /*rowsHint*/ 3);
    addAndMakeVisible (*rootKeyRow);

    // 16-step drum sequencer.
    stepSequencer = std::make_unique<StepSequencerGrid> (owner.getAPVTS());
    addAndMakeVisible (*stepSequencer);

    // FX knobs that survive on the mixer surface.
    setupKnob (chorusMix,     "Chorus",       ParamID::chorusMix);
    setupKnob (delayMix,      "Delay",        ParamID::delayMix);
    setupKnob (reverbMix,     "Reverb",       ParamID::reverbMix);
    setupKnob (shimmerVol,    "Shimmer",      ParamID::shimmerVol);
    // Shimmer audio range is 0..0.5 internally — surface that to the
    // user as 0..100% so the knob feels right at full position.
    shimmerVol.knob.textFromValueFunction = [] (double v) -> juce::String
    {
        return juce::String ((int) std::round (v * 200.0)) + "%";
    };
    shimmerVol.knob.updateText();

    setupKnob (hpfFreq,       "High Pass",    ParamID::hpfFreq);
    setupKnob (lpfFreq,       "LPF",          ParamID::lpfFreq);

    // Filter knobs format the 0..1 fader value as Hz / kHz.
    auto formatLpf = [] (double v) -> juce::String
    {
        const float hz = std::exp (4.6051702f + (float) (v * v) * 5.298375f);
        return hz < 1000.0f ? juce::String ((int) hz) + " Hz"
                            : juce::String (hz / 1000.0f, 1) + " kHz";
    };
    auto formatHpf = [] (double v) -> juce::String
    {
        if (v < 0.01) return "off";
        const float hz = std::exp (2.9957323f + (float) v * 3.218876f);
        return hz < 1000.0f ? juce::String ((int) hz) + " Hz"
                            : juce::String (hz / 1000.0f, 1) + " kHz";
    };
    lpfFreq.knob.textFromValueFunction = formatLpf;
    hpfFreq.knob.textFromValueFunction = formatHpf;
    lpfFreq.knob.updateText();
    hpfFreq.knob.updateText();

    // ── Advanced-panel knobs ─────────────────────────────────────────
    // Surfaced only when the Adv toggle is on; otherwise hidden.
    setupKnob (reverbSize,  "Size",        ParamID::reverbSize);
    reverbSize.knob.textFromValueFunction = [] (double v) -> juce::String
    {
        return juce::String ((int) std::round (1.0 + v * 9.0)) + "s";
    };
    reverbSize.knob.updateText();
    setupKnob (reverbMod,   "Mod",         ParamID::reverbMod);

    setupKnob (delayFb,     "Feedback",    ParamID::delayFb);
    setupKnob (delayTimeMs, "Div",         ParamID::delayDiv);
    {
        delayTimeMs.knob.textFromValueFunction = [] (double v) -> juce::String
        {
            static const juce::StringArray names { "1/32", "1/16", "1/16.", "1/8", "1/8.", "1/4", "1/4." };
            return names[juce::jlimit (0, names.size() - 1, (int) std::round (v))];
        };
        delayTimeMs.knob.updateText();
    }
    setupKnob (delayTone,   "Tone",        ParamID::delayTone);

    setupKnob (eqLow,       "Low",         ParamID::eqLow,    " dB");
    setupKnob (eqLoMid,     "Lo-Mid",      ParamID::eqLoMid,  " dB");
    setupKnob (eqHiMid,     "Hi-Mid",      ParamID::eqHiMid,  " dB");
    setupKnob (eqHigh,      "High",        ParamID::eqHigh,   " dB");

    auto setAdvancedKnobsVisible = [this] (bool on)
    {
        for (auto* k : { &reverbSize, &reverbMod,
                         &delayFb, &delayTimeMs, &delayTone,
                         &eqLow, &eqLoMid, &eqHiMid, &eqHigh })
        {
            k->label.setVisible (on);
            k->knob .setVisible (on);
        }
    };
    setAdvancedKnobsVisible (false);

    addAndMakeVisible (advButton);
    advButton.setClickingTogglesState (true);
    advButton.setColour (juce::TextButton::buttonColourId,
                         juce::Colour (NorcoastLookAndFeel::kPanelBg));
    advButton.setColour (juce::TextButton::buttonOnColourId,
                         juce::Colour (NorcoastLookAndFeel::kAccent).withAlpha (0.4f));
    advButton.onClick = [this, setAdvancedKnobsVisible]
    {
        advExpanded = advButton.getToggleState();
        setAdvancedKnobsVisible (advExpanded);
        // Hide the mixer's faders + FX knobs when Advanced is on.
        for (auto* k : { &foundationVol, &padsVol, &padsVol2, &textureVol,
                         &arpVol, &drumVol, &lpfFreq, &masterVol,
                         &reverbMix, &shimmerVol, &chorusMix, &delayMix,
                         &widthMod, &satAmt, &hpfFreq })
        {
            k->label.setVisible (! advExpanded);
            k->knob .setVisible (! advExpanded);
        }
        for (auto* b : { &subOctButton, &padsSubOctButton, &pads2SubOctButton,
                         &textureOctButton,
                         &foundationMuteBtn, &padsMuteBtn, &pads2MuteBtn,
                         &textureMuteBtn, &arpMuteBtn, &drumMuteBtn })
            b->setVisible (! advExpanded);
        if (arpOctavesRow != nullptr)
            arpOctavesRow->setVisible (! advExpanded);
        resized();
        repaint();
    };

    setupKnob (widthMod,      "Width",        ParamID::widthMod);
    setupKnob (satAmt,        "Saturation",   ParamID::satAmt);
    setupKnob (masterVol,     "Master",       ParamID::masterVol);

    setupKnob (arpVol,        "Arp",          ParamID::arpVol);

    // Arp octaves as a 3-pill button row (-1 / Mid / +1 shift).
    arpOctavesRow = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::arpOctaves,
        juce::Colour (0xff7eb6d4));
    addAndMakeVisible (*arpOctavesRow);

    setupKnob (drumVol,       "Movement",     ParamID::drumVol);
    // Mute button label below the fader already says "M"; the fader
    // label says "Movement" so the strip self-identifies on stage.

    // Voice / Pattern pickers as pill button rows (knobs are awkward
    // for short discrete choices).
    arpVoiceRow    = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::arpVoice,
        juce::Colour (0xff7eb6d4));    // arp blue
    drumPatternRow = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::drumPattern,
        juce::Colour (0xffe8a45e),     // movement orange
        /*rowsHint*/ 1,
        /*skipFirstN*/ 1);              // hide "Off" — fader mutes instead
    addAndMakeVisible (*arpVoiceRow);
    addAndMakeVisible (*drumPatternRow);

    // Arp rate as a button row (was a knob — too small to grab on
    // stage). Same purple as the rest of the ARP strip.
    arpRateRow = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::arpRate,
        juce::Colour (0xff9b7fd4));
    addAndMakeVisible (*arpRateRow);

    // Evolve bars as buttons too — replaces the squished "Bars" knob.
    evolveBarsRow = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::evolveBars,
        juce::Colour (0xffc4915e));
    addAndMakeVisible (*evolveBarsRow);

    // 4/4 vs 6/8 toggle — sits beside the drum pattern pills so the
    // time signature is always visible and easily switchable on stage.
    timeSigRow = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::timeSig,
        juce::Colour (0xffe8a45e));    // drum/movement orange
    addAndMakeVisible (*timeSigRow);

    // ── BPM display + manual control ─────────────────────────────────
    addAndMakeVisible (bpmLabel);
    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    bpmLabel.setColour (juce::Label::textColourId,
                        juce::Colour (NorcoastLookAndFeel::kTextDim));

    addAndMakeVisible (bpmSlider);
    bpmSlider.setSliderStyle (juce::Slider::IncDecButtons);
    bpmSlider.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_AutoDirection);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 56, 22);
    bpmSlider.setLookAndFeel (&laf);
    bpmAttach = std::make_unique<SliderAttach> (
        owner.getAPVTS(), ParamID::bpm, bpmSlider);
    bpmSlider.textFromValueFunction = [] (double v) -> juce::String
    {
        return juce::String ((int) std::round (v));
    };
    bpmSlider.updateText();

    // ── Pads 2 fader (new alt-pad layer) ──────────────────────────────
    setupKnob (padsVol2,      "Aurora",       ParamID::padsVol2);

    // ── Convert layer / output knobs into vertical faders for the
    // bottom-half mixer surface. The knobs were already wired by
    // setupKnob earlier; we just swap their slider style and dump the
    // text box (the value sits in a tooltip via the slider's
    // showLabelOnDrag behaviour).
    for (auto* k : { &foundationVol, &padsVol, &padsVol2, &textureVol,
                     &arpVol, &drumVol, &lpfFreq, &masterVol })
    {
        k->knob.setSliderStyle (juce::Slider::LinearVertical);
        k->knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
    }

    // ── Mute buttons for the 6 audio layers ──────────────────────────
    auto setupMute = [this] (juce::TextButton& btn,
                             std::unique_ptr<ButtonAttach>& attach,
                             const juce::String& paramID,
                             juce::Colour onColour)
    {
        addAndMakeVisible (btn);
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::buttonColourId,
                       juce::Colour (NorcoastLookAndFeel::kPanelBg));
        btn.setColour (juce::TextButton::buttonOnColourId, onColour);
        btn.setColour (juce::TextButton::textColourOnId,
                       juce::Colour (NorcoastLookAndFeel::kBg));
        attach = std::make_unique<ButtonAttach> (
            owner.getAPVTS(), paramID, btn);
    };
    setupMute (foundationMuteBtn, foundationMuteAttach, ParamID::foundationMute, juce::Colour (0xffd46b8a));
    setupMute (padsMuteBtn,       padsMuteAttach,       ParamID::padsMute,       juce::Colour (0xffd46b8a));
    setupMute (pads2MuteBtn,      pads2MuteAttach,      ParamID::pads2Mute,      juce::Colour (0xffd46b8a));
    setupMute (textureMuteBtn,    textureMuteAttach,    ParamID::textureMute,    juce::Colour (0xffd46b8a));
    setupMute (arpMuteBtn,        arpMuteAttach,        ParamID::arpMute,        juce::Colour (0xffd46b8a));
    setupMute (drumMuteBtn,       drumMuteAttach,       ParamID::drumMute,       juce::Colour (0xffd46b8a));

    // (Param-only knobs from earlier UI revisions have been removed
    // from the editor entirely; their APVTS params still drive the
    // audio at their stored values.)

    // Default custom-chord and sequencer panels to hidden.
    if (customDegreesRow != nullptr) customDegreesRow->setVisible (false);
    if (stepSequencer    != nullptr) stepSequencer   ->setVisible (false);

    setSize (1000, 720);

    // Give the qwerty keyboard focus so computer-keyboard keys map to MIDI.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (&qwertyKeyboard)]
    {
        if (safeThis != nullptr) safeThis->grabKeyboardFocus();
    });
}

NorcoastAmbienceEditor::~NorcoastAmbienceEditor()
{
    setLookAndFeel (nullptr);

    // Detach the LAF from every ParamKnob we own so the slider
    // destructors don't dereference a dangling LAF pointer.
    for (auto* k : { &foundationVol, &padsVol, &padsVol2, &textureVol,
                     &arpVol, &drumVol, &lpfFreq, &masterVol,
                     &reverbMix, &shimmerVol, &chorusMix,
                     &delayMix, &widthMod, &satAmt,
                     &hpfFreq,
                     &reverbSize, &reverbMod,
                     &delayFb, &delayTimeMs, &delayTone,
                     &eqLow, &eqLoMid, &eqHiMid, &eqHigh })
        k->knob.setLookAndFeel (nullptr);
    bpmSlider.setLookAndFeel (nullptr);
}

void NorcoastAmbienceEditor::applyFactoryPreset (int idx)
{
    const auto& list = Presets::factory();
    if (idx < 0 || idx >= (int) list.size()) return;
    Presets::apply (owner.getAPVTS(), list[(size_t) idx]);
}

void NorcoastAmbienceEditor::savePresetToFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Save preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("Norcoast Ambience"),
        "*.ncpre");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File()) return;
            file.getParentDirectory().createDirectory();

            auto state = owner.getAPVTS().copyState();
            if (auto xml = state.createXml())
                file.withFileExtension ("ncpre").replaceWithText (xml->toString());
        });
}

void NorcoastAmbienceEditor::loadPresetFromFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("Norcoast Ambience"),
        "*.ncpre");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (! file.existsAsFile()) return;
            if (auto xml = juce::parseXML (file))
                if (xml->hasTagName (owner.getAPVTS().state.getType()))
                    owner.getAPVTS().replaceState (juce::ValueTree::fromXml (*xml));
        });
}

void NorcoastAmbienceEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (NorcoastLookAndFeel::kBg), 0.0f, 0.0f,
                             juce::Colour (NorcoastLookAndFeel::kBgEdge),
                             (float) getWidth(), (float) getHeight(), false);
    bg.addColour (0.45, juce::Colour (NorcoastLookAndFeel::kBgMid));
    g.setGradientFill (bg);
    g.fillAll();

    // Logo: small accent-orange circle with a sine wave arc through it.
    {
        const float cx = 36.0f, cy = 38.0f, r = 14.0f;
        g.setColour (juce::Colour (NorcoastLookAndFeel::kAccent));
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.5f);

        juce::Path wave;
        const float w = r * 1.4f;
        wave.startNewSubPath (cx - w, cy);
        wave.cubicTo (cx - w * 0.4f, cy - w * 0.55f,
                      cx + w * 0.4f, cy + w * 0.55f,
                      cx + w,        cy);
        g.setColour (juce::Colour (NorcoastLookAndFeel::kAccent).withAlpha (0.85f));
        g.strokePath (wave, juce::PathStrokeType (1.6f));
    }

    // ── Top-half coloured strips ─────────────────────────────────────
    // Each control row gets its own section accent, mirroring the web
    // app's "colour codes the layer" design language.
    auto paintStrip = [&] (juce::Rectangle<int> r, juce::Colour accent, const juce::String& title)
    {
        if (r.isEmpty()) return;
        const auto rf = r.toFloat();
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelBg).withAlpha (0.6f));
        g.fillRoundedRectangle (rf, 4.0f);
        g.setColour (accent.withAlpha (0.55f));
        g.fillRect (rf.withWidth (3.0f));         // colour bar on the left
        g.setColour (accent);
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        g.drawText (title, r.reduced (12, 0).withWidth (60),
                    juce::Justification::centredLeft);
    };
    paintStrip (evolveStripBounds, juce::Colour (0xffc4915e), "EVOLVE");
    paintStrip (drumsStripBounds,  juce::Colour (0xffe8a45e), "MOVEMENT");
    paintStrip (arpStripBounds,    juce::Colour (0xff9b7fd4), "ARP");

    // ── Mixer surface backplane (the nanoKONTROL strip) ──────────────
    if (! mixerPanelBounds.isEmpty())
    {
        const auto r = mixerPanelBounds.toFloat();
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelBg));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelEdge));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);

        // Sub-panel behind the 8 FX knobs — slightly tinted so the
        // knobs feel like they live in their own little FX rack
        // separate from the layer faders below.
        const float kKnobsHeight = 96.0f;
        auto knobs = r.withHeight (kKnobsHeight + 8.0f).reduced (8.0f, 6.0f);
        g.setColour (juce::Colour (0xff111522).withAlpha (0.65f));
        g.fillRoundedRectangle (knobs, 5.0f);
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelEdge).withAlpha (0.4f));
        g.drawRoundedRectangle (knobs, 5.0f, 1.0f);

        // Thin separator between knobs panel and faders.
        const float sepY = r.getY() + kKnobsHeight + 4.0f;
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelEdge).withAlpha (0.6f));
        g.drawHorizontalLine ((int) sepY, r.getX() + 12.0f, r.getRight() - 12.0f);
    }
}

void NorcoastAmbienceEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16);

    // ── Title strip: logo (drawn in paint), chord header, BPM, time-sig,
    // preset bar, Save / Load / Stop. Every button on this strip is
    // the SAME height (kBtnH) and the same vertical inset.
    {
        constexpr int kStripH    = 56;
        constexpr int kBtnH      = 30;
        constexpr int kBtnInsetV = (kStripH - kBtnH) / 2;
        constexpr int kBtnW      = 56;
        constexpr int kGap       = 6;

        auto title = bounds.removeFromTop (kStripH);
        title.removeFromLeft (60);                           // logo space

        auto right = title.removeFromRight (4 * kBtnW + 4 * kGap + 110 /*preset*/)
                          .reduced (0, kBtnInsetV);
        stopButton.setBounds (right.removeFromRight (kBtnW));
        right.removeFromRight (kGap);
        saveButton.setBounds (right.removeFromRight (kBtnW));
        right.removeFromRight (kGap);
        loadButton.setBounds (right.removeFromRight (kBtnW));
        right.removeFromRight (kGap);
        presetBox.setBounds  (right.removeFromRight (110));
        right.removeFromRight (kGap);
        advButton.setBounds (right.removeFromRight (kBtnW));

        title.removeFromRight (kGap * 2);

        // 4/4 vs 6/8 toggle next to BPM.
        if (timeSigRow != nullptr)
            timeSigRow->setBounds (title.removeFromRight (96).reduced (0, kBtnInsetV));
        title.removeFromRight (kGap);

        // BPM display.
        auto bpmArea = title.removeFromRight (140).reduced (0, kBtnInsetV);
        bpmLabel .setBounds (bpmArea.removeFromLeft (40));
        bpmSlider.setBounds (bpmArea);

        chordHeader.setBounds (title);
    }
    bounds.removeFromTop (6);

    // ── Bottom half: nanoKONTROL-style mixer surface ──────────────────
    // Reserve the bottom slab; everything above it is the performance
    // controls (key grid, evolve panel, sequencer, etc.).
    constexpr int kFaderRowHeight = 220;   // fader (180) + label + mute
    constexpr int kKnobsRowHeight =  96;
    constexpr int kMixerHeight    = kKnobsRowHeight + kFaderRowHeight + 16;

    auto mixerArea = bounds.removeFromBottom (kMixerHeight);
    mixerPanelBounds = mixerArea;
    auto mixerInner  = mixerArea.reduced (12, 8);

    // ── Advanced panel — replaces the mixer when advExpanded is on ───
    // Three groups (Reverb · Delay · EQ) of knobs with section labels.
    if (advExpanded)
    {
        auto adv = mixerArea.reduced (16, 18);
        adv.removeFromTop (8);                            // breathing room

        const int third  = adv.getWidth() / 3;
        auto reverbCol = adv.removeFromLeft (third);
        auto delayCol  = adv.removeFromLeft (third);
        auto eqCol     = adv;

        auto layoutGroup = [] (juce::Rectangle<int> col,
                               std::initializer_list<ParamKnob*> knobs)
        {
            const int n = (int) knobs.size();
            const int colW = col.getWidth() / juce::jmax (1, n);
            int i = 0;
            for (auto* k : knobs)
            {
                (void) i;
                auto c = col.removeFromLeft (colW);
                k->label.setBounds (c.removeFromTop (kKnobLabelH));
                k->knob .setBounds (c.reduced (4, 6));
                ++i;
            }
        };
        layoutGroup (reverbCol, { &reverbSize, &reverbMod });
        layoutGroup (delayCol,  { &delayTimeMs, &delayFb, &delayTone });
        layoutGroup (eqCol,     { &eqLow, &eqLoMid, &eqHiMid, &eqHigh });
        // Skip the regular mixer layout below.
        qwertyKeyboard.setBounds (getWidth() - 2, getHeight() - 2, 1, 1);
        return;
    }

    // 8 columns evenly distributed across the mixer.
    auto layoutEightCols = [] (juce::Rectangle<int> r,
                                std::array<juce::Rectangle<int>, 8>& out)
    {
        const int colW = r.getWidth() / 8;
        for (int i = 0; i < 8; ++i)
            out[(size_t) i] = r.removeFromLeft (colW).reduced (3, 0);
    };

    // Top of mixer: 7 FX mix knobs (slot 8 above Master is intentionally
    // empty per request). Slot 7 holds HPF so the paired filter sweep
    // sits directly above the LPF fader.
    {
        auto knobsArea = mixerInner.removeFromTop (kKnobsRowHeight);
        std::array<juce::Rectangle<int>, 8> cols;
        layoutEightCols (knobsArea, cols);

        ParamKnob* fxKnobs[8] = {
            &reverbMix, &shimmerVol, &chorusMix, &delayMix,
            &widthMod,  &satAmt,     &hpfFreq,   nullptr
        };
        for (int i = 0; i < 8; ++i)
        {
            if (fxKnobs[i] == nullptr) continue;
            auto col = cols[(size_t) i];
            fxKnobs[i]->label.setBounds (col.removeFromTop (kKnobLabelH));
            fxKnobs[i]->knob .setBounds (col.reduced (2, 2));
        }
    }
    mixerInner.removeFromTop (8);   // separator strip drawn in paint

    // Bottom of mixer: 8 vertical faders with mute buttons + labels.
    {
        auto fadersArea = mixerInner.removeFromTop (kFaderRowHeight);
        std::array<juce::Rectangle<int>, 8> cols;
        layoutEightCols (fadersArea, cols);

        struct Strip
        {
            ParamKnob*       fader;
            juce::TextButton* mute;     // null for LPF / Master
        };
        const Strip strips[8] = {
            { &foundationVol, &foundationMuteBtn },
            { &padsVol,       &padsMuteBtn       },
            { &padsVol2,      &pads2MuteBtn      },
            { &textureVol,    &textureMuteBtn    },
            { &arpVol,        &arpMuteBtn        },
            { &drumVol,       &drumMuteBtn       },
            { &lpfFreq,       nullptr            },
            { &masterVol,     nullptr            }
        };

        // Octave-toggle overlays — one button per layer column, sits
        // above the mute pill so each layer's pitch shift lives right
        // next to its volume control. Arp gets a tiny 3-pill row
        // (-1/Mid/+1) instead of a single toggle since it's 3-state.
        juce::TextButton* octBtnPerCol[8] {
            &subOctButton, &padsSubOctButton, &pads2SubOctButton,
            &textureOctButton, nullptr /*arp uses arpOctavesRow*/,
            nullptr, nullptr, nullptr
        };

        for (int i = 0; i < 8; ++i)
        {
            auto col = cols[(size_t) i];
            strips[i].fader->label.setBounds (col.removeFromTop (kKnobLabelH));

            // Octave-toggle row.
            const int octH = 18;
            auto octRow = col.removeFromTop (octH);
            if (octBtnPerCol[i] != nullptr)
                octBtnPerCol[i]->setBounds (octRow.reduced (4, 1));
            else if (i == 4 && arpOctavesRow != nullptr)
                arpOctavesRow->setBounds (octRow.reduced (4, 1));
            col.removeFromTop (2);

            // Mute pill across the top of the fader column. Sits under
            // the label, just like the web app's mute dot above the
            // slider.
            const int muteH = 18;
            auto muteRow = col.removeFromTop (muteH);
            if (strips[i].mute != nullptr)
                strips[i].mute->setBounds (muteRow.reduced (10, 1));
            col.removeFromTop (4);

            // The fader fills the rest of the column. Reduce a touch
            // more horizontally so the slider doesn't dominate the
            // whole column width.
            strips[i].fader->knob.setBounds (col.reduced (10, 2));
        }
    }

    // ── Top half (everything above the mixer slab) ──────────────────
    bounds.removeFromBottom (10);

    // Pad keys (12-key root grid) — full width, sits at the top
    // directly under the title strip so they're the first thing the
    // player reaches for.
    if (rootKeyRow != nullptr)
    {
        const int keyH = juce::jlimit (110, 160, bounds.getHeight() - 220);
        rootKeyRow->setBounds (bounds.removeFromTop (keyH));
        bounds.removeFromTop (8);
    }

    // Helper: lay out a control strip and remember its bounds for paint().
    auto layoutStrip = [&] (juce::Rectangle<int>& outBounds, int height)
    {
        outBounds = bounds.removeFromTop (height);
        return outBounds.reduced (12, 4);     // inner padding
    };

    auto labelInside = [] (juce::Rectangle<int>& strip, int width = 64)
    {
        return strip.removeFromLeft (width);
    };

    // ── EVOLVE strip ─────────────────────────────────────────────────
    // [ EVOLVE ] [ chord-pool pills ......... ] [ Custom Chord + ]
    // The drone + evolve toggles dropped here — they default ON and
    // hardly ever change, so they live as compact pills on the right
    // of the bars row instead of consuming pole position. Bars itself
    // is now a button row (was a tiny knob, hard to grab on stage).
    {
        auto inner = layoutStrip (evolveStripBounds, 48);
        labelInside (inner);
        customChordToggleButton.setBounds (inner.removeFromRight (140).reduced (2, 4));
        inner.removeFromRight (8);
        if (chordPoolRow != nullptr) chordPoolRow->setBounds (inner.reduced (0, 2));
    }
    bounds.removeFromTop (2);

    // Custom-chord degrees row — same X bounds as the chord-pool pills
    // above, so the two rows align visually when the panel opens.
    if (customDegreesRow != nullptr)
    {
        customDegreesRow->setVisible (customChordExpanded);
        if (customChordExpanded)
        {
            auto cc = bounds.removeFromTop (28);
            // Match horizontal bounds of the chord-pool pills row above.
            cc.removeFromLeft (12 + 64);                 // strip pad + label
            cc.removeFromRight (12 + 140 + 8);           // strip pad + toggle + gap
            customDegreesRow->setBounds (cc);
            bounds.removeFromTop (4);
        }
    }

    // Drone / Evolve / Bars row removed from the surface — those are
    // now hard defaults (drone = on, evolve = on, bars = 4). The
    // params still exist and are reachable from the host's automation
    // panel, but the gigging surface doesn't need them.
    droneButton  .setVisible (false);
    evolveButton .setVisible (false);
    if (evolveBarsRow != nullptr) evolveBarsRow->setVisible (false);

    // ── DRUMS strip ──────────────────────────────────────────────────
    // 4/4 vs 6/8 toggle moved up into the title bar so it lives next
    // to BPM where time-related controls naturally cluster.
    {
        auto inner = layoutStrip (drumsStripBounds, 48);
        labelInside (inner);
        sequencerToggleButton.setBounds (inner.removeFromRight (110).reduced (2, 4));
        inner.removeFromRight (8);
        if (drumPatternRow != nullptr)
            drumPatternRow->setBounds (inner.reduced (0, 2));
    }
    bounds.removeFromTop (2);

    // Step sequencer — only visible when expanded. Bigger than before
    // so the cells are easy to click on stage.
    if (stepSequencer != nullptr)
    {
        stepSequencer->setVisible (sequencerExpanded);
        if (sequencerExpanded)
        {
            const int seqH = juce::jmin (110, juce::jmax (60, bounds.getHeight() - 220));
            stepSequencer->setBounds (bounds.removeFromTop (seqH).reduced (12, 0));
            bounds.removeFromTop (4);
        }
    }

    // ── ARP strip ────────────────────────────────────────────────────
    // [ ARP ] [ Voice 3-pill ............ ] [ Rate 5-pill ............ ]
    // Octaves moved over the Arp fader column in the mixer below.
    {
        auto inner = layoutStrip (arpStripBounds, 48);
        labelInside (inner);
        const int half = inner.getWidth() / 2;
        if (arpVoiceRow != nullptr)
            arpVoiceRow->setBounds (inner.removeFromLeft (half).reduced (0, 2));
        inner.removeFromLeft (6);
        if (arpRateRow != nullptr)
            arpRateRow->setBounds (inner.reduced (0, 2));
    }
    bounds.removeFromTop (2);
    // (EQ panel removed; eq* params still drive audio at their stored
    // values but no longer have a UI surface.)

    // Park the invisible qwerty-keyboard 1×1 in the bottom-right corner.
    qwertyKeyboard.setBounds (getWidth() - 2, getHeight() - 2, 1, 1);
}
