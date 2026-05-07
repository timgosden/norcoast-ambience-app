#include "PluginEditor.h"
#include "Parameters.h"
#include "Presets.h"
#include "ChordEvolver.h"

namespace
{
    constexpr int kKnobLabelH     = 20;     // ↑ a touch taller so the "Reverb / Shimmer / …" captions read at gigging distance.
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
    // Symmetric ±135° range so midpoint (0.5) lands at exactly 12 o'clock
    // — the previous 1.25π … 2.75π range computed thumbAngle = 2π at the
    // midpoint, mathematically equivalent to 0 but prone to float-rounding
    // drift so the tick rendered just past straight-up.
    {
        const float halfRange = juce::MathConstants<float>::pi * 0.75f;
        k.knob.setRotaryParameters (-halfRange, halfRange, true);
    }
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

        // Default tooltip = "Display Name (paramID) — drag, scroll,
        // or double-click to reset". The constructor caller can
        // overwrite later if it wants something more specific.
        k.knob .setTooltip (displayName + " — drag / scroll to set; double-click to reset");
        k.label.setTooltip (displayName);
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
        const bool nowStopped = stopButton.getToggleState();
        owner.setStopped (nowStopped);
        // Label flip so the next click's intent is obvious: when it's
        // stopped, the button is the way to start again.
        stopButton.setButtonText (nowStopped ? "Start" : "Stop");
    };
    stopButton.setTooltip ("Fade the dry sources to silence over the Fade Time set on the Advanced page. Reverb / delay tails ring out past the fade. Click again to fade back in.");
    saveButton.setTooltip ("Save the current state as a preset.");
    loadButton.setTooltip ("Load a preset from disk.");
    presetBox .setTooltip ("Browse the factory presets.");
    bpmLabel  .setTooltip ("Tempo (BPM). When the host provides a playhead tempo, this value is overridden.");
    bpmSlider .setTooltip ("Tempo (BPM). Drag, scroll, or click the +/- buttons. Default 70.");

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
    // All oct toggles use the same accent so the row reads as a uniform
    // strip — previously each layer had a different hue which made the
    // row feel chaotic.
    const juce::Colour kOctAccent (0xffc4915e);   // accent amber for all
    setupOctToggle (subOctButton,      ParamID::foundationSubOct,
                    subOctAttach,      kOctAccent);
    setupOctToggle (padsSubOctButton,  ParamID::padsSubOct,
                    padsSubOctAttach,  kOctAccent);
    setupOctToggle (pads2SubOctButton, ParamID::pads2SubOct,
                    pads2SubOctAttach, kOctAccent);
    setupOctToggle (textureOctButton,  ParamID::textureOctUp,
                    textureOctAttach,  kOctAccent);

    addAndMakeVisible (evolveButton);
    evolveButton.setClickingTogglesState (true);
    evolveButton.setColour (juce::TextButton::buttonColourId,
                            juce::Colour (NorcoastLookAndFeel::kPanelBg));
    evolveButton.setColour (juce::TextButton::buttonOnColourId,
                            juce::Colour (0xffc4915e));   // chord-evolve amber
    evolveButton.setColour (juce::TextButton::textColourOnId,
                            juce::Colour (NorcoastLookAndFeel::kBg));
    evolveButton.setTooltip ("Evolve: auto-cycle through enabled chord types every N bars.");
    evolveAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::evolveOn, evolveButton);
    // Hidden by default; revealed when the user opens the Advanced panel.
    evolveButton.setVisible (false);

    addAndMakeVisible (droneButton);
    droneButton.setClickingTogglesState (true);
    droneButton.setColour (juce::TextButton::buttonColourId,
                           juce::Colour (NorcoastLookAndFeel::kPanelBg));
    droneButton.setColour (juce::TextButton::buttonOnColourId,
                           juce::Colour (0xff8a6fb3));   // a softer purple
    droneButton.setColour (juce::TextButton::textColourOnId,
                           juce::Colour (NorcoastLookAndFeel::kBg));
    droneButton.setTooltip ("Drone: hold the home root continuously underneath whatever you play.");
    droneAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::droneOn, droneButton);
    droneButton.setVisible (false);

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

            // Sequencer and Custom Chord panels share the vertical
            // budget below the EVOLVE strip. Open one and the other
            // closes — opening both at once stomps the surrounding
            // strips off-screen.
            if (open)
            {
                if (flagRefPtr == &customChordExpanded && sequencerExpanded)
                {
                    sequencerExpanded = false;
                    sequencerToggleButton.setToggleState (false, juce::dontSendNotification);
                    sequencerToggleButton.setButtonText ("Sequencer +");
                }
                else if (flagRefPtr == &sequencerExpanded && customChordExpanded)
                {
                    customChordExpanded = false;
                    customChordToggleButton.setToggleState (false, juce::dontSendNotification);
                    customChordToggleButton.setButtonText ("Custom Chord +");
                }
            }
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
                 juce::Colour (0xff5eb88a),     // green — matches MOVEMENT strip
                 &sequencerExpanded);

    // ── Preset bar ──────────────────────────────────────────────────────
    addAndMakeVisible (presetBox);
    presetBox.setColour (juce::ComboBox::backgroundColourId,
                         juce::Colour (NorcoastLookAndFeel::kPanelBg));
    presetBox.setColour (juce::ComboBox::textColourId,
                         juce::Colour (NorcoastLookAndFeel::kAccent));
    presetBox.setColour (juce::ComboBox::arrowColourId,
                         juce::Colour (NorcoastLookAndFeel::kAccent));
    rebuildPresetMenu();
    presetBox.setText ("Preset", juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int picked = presetBox.getSelectedId();
        if (picked <= 0)
        {
            deleteButton.setEnabled (false);
            return;
        }

        // Delete is only meaningful for user presets.
        deleteButton.setEnabled (picked >= kUserItemBase);

        if (picked < kUserItemBase)
        {
            // Factory: 1..N
            owner.setCurrentProgram (picked - 1);
        }
        else
        {
            // User: kUserItemBase + index into userPresetFiles
            const int idx = picked - kUserItemBase;
            if (idx >= 0 && idx < userPresetFiles.size())
            {
                const auto file = userPresetFiles.getReference (idx);
                if (auto xml = juce::parseXML (file))
                    if (xml->hasTagName (owner.getAPVTS().state.getType()))
                        replaceStatePreservingPerformanceParams (
                            juce::ValueTree::fromXml (*xml));
            }
        }
    };

    addAndMakeVisible (saveButton);
    saveButton.setTooltip ("Save current state as a user preset (added to the Preset menu).");
    saveButton.onClick = [this] { promptSaveUserPreset(); };
    addAndMakeVisible (loadButton);
    loadButton.setTooltip ("Load a preset .ncpre file from disk.");
    loadButton.onClick = [this] { loadPresetFromFile(); };

    addAndMakeVisible (deleteButton);
    deleteButton.setTooltip ("Delete the currently selected user preset.");
    deleteButton.setEnabled (false);
    deleteButton.onClick = [this] { confirmDeleteSelectedUserPreset(); };

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

    // Pro-Q-style EQ curve display — shown on the Advanced panel.
    eqCurve = std::make_unique<EqCurveDisplay> (owner.getAPVTS());
    addAndMakeVisible (*eqCurve);
    eqCurve->setVisible (false);   // only visible when Advanced is open

    // FX knobs that survive on the mixer surface.
    setupKnob (chorusMix,     "Chorus",       ParamID::chorusMix);
    setupKnob (delayMix,      "Delay",        ParamID::delayMix);
    setupKnob (reverbMix,     "Reverb",       ParamID::reverbMix);
    setupKnob (shimmerVol,    "Shimmer",      ParamID::shimmerVol);
    // Param range is 0..0.5 internally (locked for preset compat) but
    // the player sees a normal 0..100% knob — scale display by 200 so
    // full slider reads 100%. Matching audio scale of 2.0x is applied
    // in the processor.
    shimmerVol.knob.textFromValueFunction = [] (double v) -> juce::String
    {
        return juce::String ((int) std::round (v * 200.0)) + "%";
    };
    shimmerVol.knob.updateText();

    setupKnob (hpfFreq,       "HPF",          ParamID::hpfFreq);
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

    // Stop / key transition timing controls — surfaced on Advanced.
    setupKnob (fadeTime,  "Stop Fade", ParamID::fadeTime);
    fadeTime.knob.textFromValueFunction = [] (double v) -> juce::String
    {
        return juce::String (v, 1) + " s";
    };
    fadeTime.knob.updateText();

    setupKnob (keyXfade,  "Chord Xfade", ParamID::keyXfade);
    keyXfade.knob.setTooltip ("How long held notes take to fade out when the chord type evolves to a new shape — also covers manual key changes. Short = punchy edits, long = smooth bloom.");
    keyXfade.knob.textFromValueFunction = [] (double v) -> juce::String
    {
        return juce::String (v, 2) + " s";
    };
    keyXfade.knob.updateText();
    // (Tried LinearBarVertical here — looked wrong for a bipolar param,
    // since the bar filled from the bottom regardless of cut vs boost.
    // Keeping rotaries; a dedicated EQ-curve visualiser is a bigger
    // task tracked separately.)

    auto setAdvancedKnobsVisible = [this] (bool on)
    {
        for (auto* k : { &reverbSize, &reverbMod,
                         &delayFb, &delayTimeMs, &delayTone,
                         &fadeTime, &keyXfade })
        {
            k->label.setVisible (on);
            k->knob .setVisible (on);
        }
        // EQ knobs stay hidden — the EqCurveDisplay drives them now.
        for (auto* k : { &eqLow, &eqLoMid, &eqHiMid, &eqHigh })
        {
            k->label.setVisible (false);
            k->knob .setVisible (false);
        }
        if (eqCurve != nullptr) eqCurve->setVisible (on);
    };
    setAdvancedKnobsVisible (false);

    addAndMakeVisible (advButton);
    advButton.setClickingTogglesState (true);
    advButton.setColour (juce::TextButton::buttonColourId,
                         juce::Colour (NorcoastLookAndFeel::kPanelBg));
    advButton.setColour (juce::TextButton::buttonOnColourId,
                         juce::Colour (0xffb07acc));   // EQ purple — "page" cue
    advButton.setColour (juce::TextButton::textColourOnId,
                         juce::Colour (NorcoastLookAndFeel::kBg));
    advButton.setTooltip ("Advanced controls (Reverb Size / Mod, Delay Div / FB / Tone, Master EQ)");
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
        lpfHzLabel.setVisible (! advExpanded);
        // Evolve / Drone on/off pills live on the Advanced surface —
        // hidden on the mixer view, visible when Advanced is open.
        evolveButton.setVisible (advExpanded);
        droneButton .setVisible (advExpanded);
        resized();
        repaint();
    };

    setupKnob (widthMod,      "Width",        ParamID::widthMod);
    setupKnob (satAmt,        "Drive",        ParamID::satAmt);
    setupKnob (masterVol,     "Master",       ParamID::masterVol);

    setupKnob (arpVol,        "Arp",          ParamID::arpVol);

    // Arp octaves as a 3-pill button row (-1 / Mid / +1 shift).
    arpOctavesRow = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::arpOctaves,
        juce::Colour (0xff9b7fd4));   // ARP purple — matches the strip
    addAndMakeVisible (*arpOctavesRow);

    setupKnob (drumVol,       "Movement",     ParamID::drumVol);
    // Mute button label below the fader already says "M"; the fader
    // label says "Movement" so the strip self-identifies on stage.

    // Voice / Pattern pickers as pill button rows (knobs are awkward
    // for short discrete choices).
    arpVoiceRow    = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::arpVoice,
        juce::Colour (0xff9b7fd4));    // ARP purple — uniform with strip
    drumPatternRow = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::drumPattern,
        juce::Colour (0xff5eb88a),     // movement green (matches strip)
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
        juce::Colour (0xffe8a45e));
    addAndMakeVisible (*timeSigRow);
    // Distinct active hues so 4/4 vs 6/8 reads at a glance:
    //   4/4 → green (foundation/movement family)
    //   6/8 → blue  (arp / waltz mood)
    timeSigRow->setButtonAccent (0, juce::Colour (0xff5eb88a));
    timeSigRow->setButtonAccent (1, juce::Colour (0xff7eb6d4));

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
        k->knob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);   // no % under fader
        // Listen for drag so we can float a dB readout above the thumb.
        k->knob.addListener (this);
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

    // ── LPF Hz indicator ─────────────────────────────────────────────
    // Sits in the slot above the LPF fader where every other column has
    // its octave-toggle button. Live-updated from the LPF slider value.
    addAndMakeVisible (lpfHzLabel);
    lpfHzLabel.setJustificationType (juce::Justification::centred);
    lpfHzLabel.setColour (juce::Label::textColourId,
                          juce::Colour (NorcoastLookAndFeel::kAccent));
    lpfHzLabel.setInterceptsMouseClicks (false, false);
    lpfHzLabel.setText ("--", juce::dontSendNotification);

    // (Param-only knobs from earlier UI revisions have been removed
    // from the editor entirely; their APVTS params still drive the
    // audio at their stored values.)

    // Default custom-chord and sequencer panels to hidden.
    if (customDegreesRow != nullptr) customDegreesRow->setVisible (false);
    if (stepSequencer    != nullptr) stepSequencer   ->setVisible (false);

    setSize (1000, 720);

    // Drive the per-fader level meters at ~24 Hz refresh.
    startTimerHz (24);

    // Give the qwerty keyboard focus so computer-keyboard keys map to MIDI.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (&qwertyKeyboard)]
    {
        if (safeThis != nullptr) safeThis->grabKeyboardFocus();
    });
}

void NorcoastAmbienceEditor::sliderDragStarted (juce::Slider* s)
{
    activeFader = s;
    repaint();
}

void NorcoastAmbienceEditor::sliderDragEnded (juce::Slider* s)
{
    if (activeFader == s) activeFader = nullptr;
    repaint();
}

void NorcoastAmbienceEditor::sliderValueChanged (juce::Slider* s)
{
    if (s == activeFader) repaint();
}

void NorcoastAmbienceEditor::timerCallback()
{
    // Level meters were removed; the processor still writes
    // layerLevels[] but no editor surface reads it now.

    // Visual Stop fade — eases toward 0 while stopped, back to 1 when
    // released. Frame-time-independent (uses real ms delta) so it
    // matches the audio fade regardless of timer jitter.
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float  dt  = lastVisualStopUpdateMs > 0.0
                            ? (float) ((now - lastVisualStopUpdateMs) / 1000.0)
                            : 0.0f;
        lastVisualStopUpdateMs = now;

        const float fadeTime = juce::jmax (0.05f,
            owner.getAPVTS().getRawParameterValue (ParamID::fadeTime)->load());
        const float target = owner.isStopped() ? 0.0f : 1.0f;
        const float step   = dt / fadeTime;
        const float prev   = visualStopMult;
        if (visualStopMult < target)
            visualStopMult = juce::jmin (target, visualStopMult + step);
        else if (visualStopMult > target)
            visualStopMult = juce::jmax (target, visualStopMult - step);
        if (std::abs (visualStopMult - prev) > 1e-3f)
        {
            // Push the new multiplier directly onto the 6 source-layer
            // FaderSlider instances. Master + LPF are intentionally
            // excluded (master isn't a "source", LPF is a filter
            // cutoff, not a level). Calling repaint on each forces
            // FaderSlider::paint to re-run with the new multiplier.
            FaderSlider* sources[6] = {
                &foundationVol.knob, &padsVol.knob, &padsVol2.knob, &textureVol.knob,
                &arpVol.knob,        &drumVol.knob
            };
            for (auto* s : sources)
            {
                s->stopFadeMult = visualStopMult;
                s->repaint();
            }
        }
    }

    // Live LPF Hz readout above the fader — same Hz/kHz mapping as the
    // slider's textFromValueFunction.
    {
        const float v = (float) lpfFreq.knob.getValue();
        const float hz = std::exp (4.6051702f + (v * v) * 5.298375f);
        const juce::String txt = hz < 1000.0f
            ? juce::String ((int) hz) + " Hz"
            : juce::String (hz / 1000.0f, 1) + " kHz";
        if (lpfHzLabel.getText() != txt)
            lpfHzLabel.setText (txt, juce::dontSendNotification);
    }

    // Custom-chord pill labels follow the home root: 1·C, 2·D, 3·E, …
    // Each key uses its conventional spelling — sharps in sharp keys
    // (G major has F#, not Gb), flats in flat keys (Eb major has Bb,
    // not A#). Gb major uses Cb because that's the proper diatonic
    // 4th — this is the only place Cb shows up.
    if (customDegreesRow != nullptr)
    {
        const int rootIdx = juce::jlimit (0, 11,
            (int) owner.getAPVTS().getRawParameterValue (ParamID::homeRoot)->load());
        if (rootIdx != lastHomeRootForLabels)
        {
            // 12 rows × 7 cols — the major-scale spelling for each
            // home-root choice. homeRoot order matches Parameters.h:
            //   C, Db, D, Eb, E, F, Gb, G, Ab, A, Bb, B.
            static const char* kMajorScale[12][7] = {
                /* C  */ { "C",  "D",  "E",  "F",  "G",  "A",  "B"  },
                /* Db */ { "Db", "Eb", "F",  "Gb", "Ab", "Bb", "C"  },
                /* D  */ { "D",  "E",  "F#", "G",  "A",  "B",  "C#" },
                /* Eb */ { "Eb", "F",  "G",  "Ab", "Bb", "C",  "D"  },
                /* E  */ { "E",  "F#", "G#", "A",  "B",  "C#", "D#" },
                /* F  */ { "F",  "G",  "A",  "Bb", "C",  "D",  "E"  },
                /* Gb */ { "Gb", "Ab", "Bb", "Cb", "Db", "Eb", "F"  },
                /* G  */ { "G",  "A",  "B",  "C",  "D",  "E",  "F#" },
                /* Ab */ { "Ab", "Bb", "C",  "Db", "Eb", "F",  "G"  },
                /* A  */ { "A",  "B",  "C#", "D",  "E",  "F#", "G#" },
                /* Bb */ { "Bb", "C",  "D",  "Eb", "F",  "G",  "A"  },
                /* B  */ { "B",  "C#", "D#", "E",  "F#", "G#", "A#" },
            };
            for (int i = 0; i < 7; ++i)
                customDegreesRow->setLabel (i,
                    juce::String (i + 1) + juce::String::fromUTF8 (" \xc2\xb7 ")
                                         + kMajorScale[rootIdx][i]);
            lastHomeRootForLabels = rootIdx;
        }
    }
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
                     &fadeTime, &keyXfade,
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

juce::File NorcoastAmbienceEditor::getUserPresetDir() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile ("Norcoast Ambience")
                .getChildFile ("Presets");
}

void NorcoastAmbienceEditor::rebuildPresetMenu (int selectFactoryIdx,
                                                int selectUserIdx)
{
    presetBox.clear (juce::dontSendNotification);

    // Factory section.
    auto* root = presetBox.getRootMenu();
    if (root != nullptr)
        root->addSectionHeader ("Factory");
    int id = 1;
    for (const auto& preset : Presets::factory())
        presetBox.addItem (preset.name, id++);

    // Re-scan the user-preset directory.
    userPresetFiles.clear();
    const auto dir = getUserPresetDir();
    if (dir.isDirectory())
    {
        juce::Array<juce::File> found;
        dir.findChildFiles (found, juce::File::findFiles, /*recursive*/ false,
                            "*.ncpre");
        // Stable alphabetical order so the menu doesn't shuffle.
        std::sort (found.begin(), found.end(),
                   [] (const juce::File& a, const juce::File& b)
                   {
                       return a.getFileNameWithoutExtension()
                                .compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
                   });
        userPresetFiles = std::move (found);
    }

    if (! userPresetFiles.isEmpty())
    {
        if (root != nullptr)
            root->addSectionHeader ("User");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(),
                               kUserItemBase + i);
    }

    if (selectFactoryIdx >= 0)
        presetBox.setSelectedId (selectFactoryIdx + 1, juce::dontSendNotification);
    else if (selectUserIdx >= 0)
        presetBox.setSelectedId (kUserItemBase + selectUserIdx,
                                 juce::dontSendNotification);
}

void NorcoastAmbienceEditor::confirmDeleteSelectedUserPreset()
{
    const int picked = presetBox.getSelectedId();
    if (picked < kUserItemBase) return;
    const int idx = picked - kUserItemBase;
    if (idx < 0 || idx >= userPresetFiles.size()) return;

    const auto file = userPresetFiles.getReference (idx);
    const auto name = file.getFileNameWithoutExtension();

    deletePromptWindow = std::make_unique<juce::AlertWindow> (
        "Delete preset",
        "Delete \"" + name + "\"?\nThis cannot be undone.",
        juce::MessageBoxIconType::WarningIcon);
    deletePromptWindow->addButton ("Delete", 1, juce::KeyPress (juce::KeyPress::returnKey));
    deletePromptWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    deletePromptWindow->enterModalState (true,
        juce::ModalCallbackFunction::create (
            [this, file] (int result)
            {
                deletePromptWindow.reset();
                if (result != 1) return;

                file.deleteFile();
                rebuildPresetMenu();
                presetBox.setText ("Preset", juce::dontSendNotification);
                deleteButton.setEnabled (false);
            }));
}

void NorcoastAmbienceEditor::promptSaveUserPreset()
{
    savePromptWindow = std::make_unique<juce::AlertWindow> (
        "Save user preset",
        "Enter a name for this preset. It will appear in the Preset menu.",
        juce::MessageBoxIconType::QuestionIcon);

    savePromptWindow->addTextEditor ("name", "My Preset", "Name:");
    savePromptWindow->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    savePromptWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    savePromptWindow->enterModalState (true,
        juce::ModalCallbackFunction::create (
            [this] (int result)
            {
                if (result != 1 || savePromptWindow == nullptr)
                {
                    savePromptWindow.reset();
                    return;
                }

                auto name = savePromptWindow->getTextEditorContents ("name").trim();
                savePromptWindow.reset();

                if (name.isEmpty()) return;
                // Sanitise — strip path-illegal characters so the
                // filename matches what the user typed.
                name = juce::File::createLegalFileName (name);
                if (name.isEmpty()) return;

                const auto dir = getUserPresetDir();
                dir.createDirectory();
                auto file = dir.getChildFile (name).withFileExtension ("ncpre");

                auto state = owner.getAPVTS().copyState();
                if (auto xml = state.createXml())
                    file.replaceWithText (xml->toString());

                rebuildPresetMenu();

                // Select the freshly-saved preset so the dropdown
                // reflects it as the active state.
                for (int i = 0; i < userPresetFiles.size(); ++i)
                    if (userPresetFiles[i] == file)
                    {
                        presetBox.setSelectedId (kUserItemBase + i,
                                                 juce::dontSendNotification);
                        break;
                    }
            }));
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
                    replaceStatePreservingPerformanceParams (
                        juce::ValueTree::fromXml (*xml));
        });
}

void NorcoastAmbienceEditor::replaceStatePreservingPerformanceParams (juce::ValueTree newState)
{
    // Snapshot the live performance params, swap state, then put them
    // back so a preset never recalls the player's key / BPM / time
    // signature / global EQ.
    auto& vts = owner.getAPVTS();
    const float eqLo = vts.getRawParameterValue (ParamID::eqLow)   ->load();
    const float eqLm = vts.getRawParameterValue (ParamID::eqLoMid) ->load();
    const float eqHm = vts.getRawParameterValue (ParamID::eqHiMid) ->load();
    const float eqHi = vts.getRawParameterValue (ParamID::eqHigh)  ->load();
    const float root = vts.getRawParameterValue (ParamID::homeRoot)->load();
    const float bpmV = vts.getRawParameterValue (ParamID::bpm)     ->load();
    const float ts   = vts.getRawParameterValue (ParamID::timeSig) ->load();

    vts.replaceState (newState);

    auto restore = [&vts] (const char* paramID, float value)
    {
        if (auto* p = vts.getParameter (paramID))
        {
            const auto range = p->getNormalisableRange();
            p->setValueNotifyingHost (range.convertTo0to1 (value));
        }
    };
    restore (ParamID::eqLow,    eqLo);
    restore (ParamID::eqLoMid,  eqLm);
    restore (ParamID::eqHiMid,  eqHm);
    restore (ParamID::eqHigh,   eqHi);
    restore (ParamID::homeRoot, root);
    restore (ParamID::bpm,      bpmV);
    restore (ParamID::timeSig,  ts);
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
        // Web-app card look: dark slate fill, no left accent strip,
        // bigger rounded corners. The accent only colours the heading
        // text — keeps the surface clean.
        g.setColour (juce::Colour (0xff121826).withAlpha (0.85f));
        g.fillRoundedRectangle (rf, 8.0f);
        g.setColour (juce::Colour (0xff242a36));
        g.drawRoundedRectangle (rf, 8.0f, 1.0f);
        g.setColour (accent.withAlpha (0.9f));
        g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
        // Paint inside a 96-px slot so MOVEMENT (the longest label) fits
        // without truncation. Was hard-coded to 60 px.
        g.drawText (title, r.reduced (12, 0).withWidth (96),
                    juce::Justification::centredLeft);
    };
    paintStrip (evolveStripBounds, juce::Colour (0xffc4915e), "EVOLVE");
    paintStrip (drumsStripBounds,  juce::Colour (0xff5eb88a), "MOVEMENT");   // green — web-app accent
    paintStrip (arpStripBounds,    juce::Colour (0xff9b7fd4), "ARP");

    // ARP sub-labels — "VOICE" / "RATE" so the player can tell at a
    // glance which pill row is which (otherwise they're identical-
    // looking pill rows next to each other).
    if (! advExpanded)
    {
        g.setColour (juce::Colour (0xff9b7fd4).withAlpha (0.85f));
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        if (! arpVoiceLabelBounds.isEmpty())
            g.drawText ("VOICE", arpVoiceLabelBounds.toFloat(),
                        juce::Justification::centredLeft);
        if (! arpRateLabelBounds.isEmpty())
            g.drawText ("RATE", arpRateLabelBounds.toFloat(),
                        juce::Justification::centredLeft);
    }

    // "Chord Evolve every:" label on the Advanced panel, painted right
    // next to the bars choice so the user can see what the bars choice
    // actually controls. Bigger / brighter than before — at 11pt amber on
    // the slate Advanced panel it was unreadable.
    if (advExpanded && ! advBarsLabelBounds.isEmpty())
    {
        const auto rf = advBarsLabelBounds.toFloat().reduced (2.0f, 1.0f);
        g.setColour (juce::Colour (0xfff5e6d3));   // near-white cream — readable
        g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        g.drawText ("Chord Evolve every:", rf,
                    juce::Justification::centred);
    }

    // ── Mixer surface backplane (the nanoKONTROL strip) ──────────────
    if (! mixerPanelBounds.isEmpty())
    {
        const auto r = mixerPanelBounds.toFloat();
        // Web-app card: bigger corner radius, softer slate tint, much
        // dimmer border. Advanced state nudges the tint cooler and
        // tints the border purple so the page swap is obvious.
        g.setColour (advExpanded
                     ? juce::Colour (0xff141a28).withAlpha (0.92f)
                     : juce::Colour (0xff121826).withAlpha (0.85f));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (advExpanded
                     ? juce::Colour (0xffb07acc).withAlpha (0.45f)
                     : juce::Colour (0xff242a36));
        g.drawRoundedRectangle (r, 12.0f, advExpanded ? 1.4f : 1.0f);

        if (! advExpanded)
        {
            // FX rack sub-panel — distinctly tinted (cooler / darker
            // than the surrounding mixer plate). Border is now the same
            // dim slate as the rest, with a small "FX" sticker for
            // identity instead of an accent border (which read as
            // "warning" on the dark background).
            const float kKnobsHeight = 96.0f;
            auto knobs = r.withHeight (kKnobsHeight + 16.0f).reduced (8.0f, 4.0f);
            g.setColour (juce::Colour (0xff0c1019).withAlpha (0.92f));
            g.fillRoundedRectangle (knobs, 10.0f);
            // FX border in accent orange — the colour-scheme rule is
            // FX = orange, so the rack identifies as orange-bordered
            // even though the slate fill matches the surrounding mixer.
            g.setColour (juce::Colour (NorcoastLookAndFeel::kAccent).withAlpha (0.45f));
            g.drawRoundedRectangle (knobs, 10.0f, 1.2f);

            g.setColour (juce::Colour (NorcoastLookAndFeel::kAccent).withAlpha (0.7f));
            g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
            g.drawText ("FX", knobs.toNearestInt().reduced (10, 6).withHeight (12),
                        juce::Justification::topLeft);
        }
    }

}

void NorcoastAmbienceEditor::paintOverChildren (juce::Graphics& g)
{
    // ── Active-fader dB readout (drag tooltip) ──────────────────────
    // Painted in paintOverChildren so the readout sits above the fader
    // thumb instead of being hidden behind it. Same visual style as the
    // EQ band-node drag readout for consistency.
    // LPF has its own persistent Hz label above the column, so we skip
    // the floating drag tooltip for it — only the gain faders get one.
    if (activeFader != nullptr && activeFader != &lpfFreq.knob && ! advExpanded)
    {
        const auto fb = activeFader->getBounds();
        const float v = (float) activeFader->getValue();

        juce::String txt;
        if (v <= 1e-4f)
            txt = "-inf dB";
        else
        {
            const float dB = juce::Decibels::gainToDecibels (v, -60.0f);
            txt = (dB >= 0 ? "+" : "") + juce::String (dB, 1) + " dB";
        }

        const float thumbY = (float) fb.getBottom()
                           - v * (float) fb.getHeight();
        const float boxX = (float) fb.getCentreX() - 38.0f;
        const float boxY = juce::jmax (4.0f, thumbY - 22.0f);

        const juce::Rectangle<float> bg (boxX, boxY, 76.0f, 18.0f);
        g.setColour (juce::Colour (0xff0d1220).withAlpha (0.95f));
        g.fillRoundedRectangle (bg, 4.0f);
        g.setColour (juce::Colour (0xffe8a45e));
        g.drawRoundedRectangle (bg, 4.0f, 1.0f);
        g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
        g.drawText (txt, bg, juce::Justification::centred);
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

        // Five buttons + 5 gaps + 110 px preset combo.
        auto right = title.removeFromRight (5 * kBtnW + 5 * kGap + 110 /*preset*/)
                          .reduced (0, kBtnInsetV);
        stopButton.setBounds (right.removeFromRight (kBtnW));
        right.removeFromRight (kGap);
        saveButton.setBounds (right.removeFromRight (kBtnW));
        right.removeFromRight (kGap);
        loadButton.setBounds (right.removeFromRight (kBtnW));
        right.removeFromRight (kGap);
        deleteButton.setBounds (right.removeFromRight (kBtnW));
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

    // ── Top half (key grid + EVOLVE / DRUMS / ARP strips + their
    // optional Custom Chord / Sequencer panels) ──────────────────────
    // Laid out FIRST so the Custom Chord / Sequencer toggles work
    // identically whether or not Advanced mode is open, and so the
    // mixer anchors directly underneath ARP with no dead space.
    bounds.removeFromBottom (10);

    if (rootKeyRow != nullptr)
    {
        // Reserve below = strips (3*42) + mixer + small fudge. When a
        // collapsible panel (custom-chord pills / step sequencer) is
        // open, give it room by shrinking the home-root grid; otherwise
        // the panel computes 0 height and refuses to open. Sequencer
        // wants ~108 px; the chord-degree row needs ~32 px.
        int extra = 0;
        if (sequencerExpanded)   extra += 108;
        if (customChordExpanded) extra +=  32;
        // Cap key-grid max at 132 — leaves the mixer its full
        // kMixerHeight budget given 3 strips × 40 + 3 inter-strip gaps
        // × 8 + 8 px after keyRow = 152 px of fixed below-keyRow
        // furniture. The grid still scales up nicely on bigger window
        // heights but never starves the mixer.
        const int keyH = juce::jlimit (88, 132,
                                       bounds.getHeight() - (kMixerHeight + 152 + extra));
        rootKeyRow->setBounds (bounds.removeFromTop (keyH));
        bounds.removeFromTop (8);
    }

    auto layoutStrip = [&] (juce::Rectangle<int>& outBounds, int height)
    {
        outBounds = bounds.removeFromTop (height);
        return outBounds.reduced (12, 4);     // inner padding
    };

    auto labelInside = [] (juce::Rectangle<int>& strip, int width = 100)
    {
        return strip.removeFromLeft (width);
    };

    {
        auto inner = layoutStrip (evolveStripBounds, 40);
        labelInside (inner);
        customChordToggleButton.setBounds (inner.removeFromRight (140).reduced (2, 4));
        inner.removeFromRight (8);
        if (chordPoolRow != nullptr) chordPoolRow->setBounds (inner.reduced (0, 2));
    }
    bounds.removeFromTop (8);

    if (customDegreesRow != nullptr)
    {
        customDegreesRow->setVisible (customChordExpanded);
        if (customChordExpanded)
        {
            auto cc = bounds.removeFromTop (28);
            cc.removeFromLeft (12 + 64);
            cc.removeFromRight (12 + 140 + 8);
            customDegreesRow->setBounds (cc);
            bounds.removeFromTop (4);
        }
    }

    droneButton .setVisible (advExpanded);
    if (evolveBarsRow != nullptr) evolveBarsRow->setVisible (advExpanded);

    {
        auto inner = layoutStrip (drumsStripBounds, 40);
        labelInside (inner);
        sequencerToggleButton.setBounds (inner.removeFromRight (110).reduced (2, 4));
        inner.removeFromRight (8);
        if (drumPatternRow != nullptr)
            drumPatternRow->setBounds (inner.reduced (0, 2));
    }
    bounds.removeFromTop (8);

    if (stepSequencer != nullptr)
    {
        stepSequencer->setVisible (sequencerExpanded);
        if (sequencerExpanded)
        {
            const int reserveBelow = 40 + 2 + kMixerHeight;  // ARP + gap + mixer
            const int avail = juce::jmax (0, bounds.getHeight() - reserveBelow);
            // Force at least 96 px so the 16 step pads have a clickable
            // size — the rootKeyRow is shrunk above to make room.
            const int seqH  = juce::jlimit (96, 132, avail);
            stepSequencer->setBounds (bounds.removeFromTop (seqH).reduced (12, 0));
            bounds.removeFromTop (4);
        }
    }

    {
        auto inner = layoutStrip (arpStripBounds, 40);
        labelInside (inner);
        const int half = inner.getWidth() / 2;
        // Voice + Rate each get a small "VOICE" / "RATE" label in the
        // strip so the player knows what the two pill rows are. Stored
        // as Rectangles and painted in paint() below.
        auto voiceCol = inner.removeFromLeft (half);
        arpVoiceLabelBounds = voiceCol.removeFromLeft (44);
        if (arpVoiceRow != nullptr)
            arpVoiceRow->setBounds (voiceCol.reduced (0, 2));
        inner.removeFromLeft (6);
        arpRateLabelBounds = inner.removeFromLeft (40);
        if (arpRateRow != nullptr)
            arpRateRow->setBounds (inner.reduced (0, 2));
    }
    bounds.removeFromTop (8);

    // ── Mixer slab — anchored immediately under the ARP strip so
    // there's no dead band between the two halves of the editor. ───
    auto mixerArea = bounds.removeFromTop (kMixerHeight);
    mixerPanelBounds = mixerArea;
    auto mixerInner  = mixerArea.reduced (12, 8);

    // ── Advanced panel — replaces the mixer when advExpanded is on ───
    // Three groups (Reverb · Delay · EQ) of knobs with section labels.
    if (advExpanded)
    {
        // Layout: left half = 5 uniform rotary knobs (Reverb Size/Mod ·
        // Delay Div/Feedback/Tone). Right half = Pro-Q-style EQ curve
        // display.
        auto adv = mixerArea.reduced (16, 18);
        adv.removeFromTop (8);

        const int leftW  = adv.getWidth() * 5 / 9;
        auto knobsArea = adv.removeFromLeft (leftW);
        adv.removeFromLeft (12);                              // gap

        ParamKnob* knobs[] = {
            &reverbSize, &reverbMod,
            &delayTimeMs, &delayFb, &delayTone,
            &fadeTime,   &keyXfade
        };
        const int n    = (int) (sizeof (knobs) / sizeof (knobs[0]));
        // Reserve a row at the bottom for the [Evolve] [Drone] pills +
        // the chord-evolve bars choice. Bars label is painted to the
        // right of the toggles so the user reads it as "evolve every
        // N bars".
        auto barsArea = knobsArea.removeFromBottom (28);
        const int colW = knobsArea.getWidth() / n;
        // Constrain the rotary to a fixed visual size so the label +
        // rotary + value-text-below sit as a tight unit instead of the
        // rotary stretching to fill a tall column (which left a big
        // gap between the label and the knob).
        constexpr int kAdvKnobH = 84;
        for (int i = 0; i < n; ++i)
        {
            auto c = knobsArea.removeFromLeft (colW);
            knobs[i]->label.setBounds (c.removeFromTop (kKnobLabelH));
            c.removeFromTop (2);
            knobs[i]->knob .setBounds (c.removeFromTop (kAdvKnobH).reduced (4, 0));
        }
        // [Evolve] [Drone] [Chord Evolve every: ... bars row].
        // The label is painted in the Advanced paint pass so it stays
        // discoverable; the bars row itself is the chord-evolve cycle
        // length, default 4 bars.
        {
            auto evRow = barsArea;
            auto evToggle = evRow.removeFromLeft (66);
            evRow.removeFromLeft (4);
            auto drToggle = evRow.removeFromLeft (66);
            evRow.removeFromLeft (8);
            // 132 px label slot for "Chord Evolve every:" — wider so the
            // 13pt bold text isn't crushed against the bars row.
            advBarsLabelBounds = evRow.removeFromLeft (132);
            evRow.removeFromLeft (4);
            evolveButton.setBounds (evToggle.reduced (2, 2));
            droneButton .setBounds (drToggle.reduced (2, 2));
            if (evolveBarsRow != nullptr)
                evolveBarsRow->setBounds (evRow.reduced (0, 2));
        }

        if (eqCurve != nullptr)
            eqCurve->setBounds (adv.reduced (0, 4));
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

            // Octave-toggle row — taller than the original 18 px so the
            // button text isn't crushed by the LAF font multiplier.
            const int octH = 26;
            auto octRow = col.removeFromTop (octH);
            if (octBtnPerCol[i] != nullptr)
                octBtnPerCol[i]->setBounds (octRow.reduced (4, 2));
            else if (i == 4 && arpOctavesRow != nullptr)
                arpOctavesRow->setBounds (octRow.reduced (4, 2));
            else if (i == 6)
                lpfHzLabel.setBounds (octRow.reduced (2, 2));
            col.removeFromTop (3);

            // Mute pill across the top of the fader column. Sits under
            // the label, just like the web app's mute dot above the
            // slider.
            const int muteH = 24;
            auto muteRow = col.removeFromTop (muteH);
            if (strips[i].mute != nullptr)
                strips[i].mute->setBounds (muteRow.reduced (8, 2));
            col.removeFromTop (4);

            // The fader fills the rest of the column. Reduce a touch
            // more horizontally so the slider doesn't dominate the
            // whole column width.
            strips[i].fader->knob.setBounds (col.reduced (10, 2));
        }
    }

    // (Strips already laid out above the mixer.)

    // Park the invisible qwerty-keyboard 1×1 in the bottom-right corner.
    qwertyKeyboard.setBounds (getWidth() - 2, getHeight() - 2, 1, 1);
}
