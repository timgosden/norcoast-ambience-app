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

    addAndMakeVisible (subOctButton);
    subOctButton.setClickingTogglesState (true);
    subOctAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::foundationSubOct, subOctButton);

    addAndMakeVisible (textureOctButton);
    textureOctButton.setClickingTogglesState (true);
    textureOctAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::textureOctUp, textureOctButton);

    addAndMakeVisible (evolveButton);
    evolveButton.setClickingTogglesState (true);
    evolveAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::evolveOn, evolveButton);

    addAndMakeVisible (droneButton);
    droneButton.setClickingTogglesState (true);
    droneAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::droneOn, droneButton);

    // EQ collapse toggle — sits in row 2 in place of the EQ section
    // until the user clicks it, at which point the four EQ knobs swap
    // in and the surrounding sections shrink to make room.
    addAndMakeVisible (eqToggleButton);
    eqToggleButton.setClickingTogglesState (true);
    eqToggleButton.setColour (juce::TextButton::buttonColourId,
                              juce::Colour (NorcoastLookAndFeel::kPanelBg));
    eqToggleButton.setColour (juce::TextButton::buttonOnColourId,
                              juce::Colour (NorcoastLookAndFeel::kAccent).withAlpha (0.4f));
    eqToggleButton.onClick = [this]
    {
        eqExpanded = eqToggleButton.getToggleState();
        eqToggleButton.setButtonText (eqExpanded ? juce::String ("EQ ▴")
                                                 : juce::String ("EQ ▾"));
        for (auto* k : { &eqLow, &eqLoMid, &eqHiMid, &eqHigh })
        {
            k->label.setVisible (eqExpanded);
            k->knob .setVisible (eqExpanded);
        }
        resized();
        repaint();
    };
    // Initialise hidden state.
    for (auto* k : { &eqLow, &eqLoMid, &eqHiMid, &eqHigh })
    {
        k->label.setVisible (false);
        k->knob .setVisible (false);
    }

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
    setupKnob (padsVol,       "Pads",         ParamID::padsVol);
    setupKnob (textureVol,    "Texture",      ParamID::textureVol);

    setupKnob (evolveRate,    "Bars",         ParamID::evolveBars);
    {
        static const juce::StringArray barChoices { "1", "2", "4", "8", "16", "32" };
        evolveRate.knob.textFromValueFunction = [] (double v) -> juce::String
        {
            const int idx = juce::jlimit (0, barChoices.size() - 1, (int) std::round (v));
            return barChoices[idx];
        };
        evolveRate.knob.updateText();
    }

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

    // 12-key root grid (full-width row at the bottom of the editor).
    rootKeyRow = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::homeRoot,
        juce::Colour (NorcoastLookAndFeel::kAccent));
    addAndMakeVisible (*rootKeyRow);

    // 16-step drum sequencer.
    stepSequencer = std::make_unique<StepSequencerGrid> (owner.getAPVTS());
    addAndMakeVisible (*stepSequencer);

    setupKnob (chorusMix,     "Chorus",       ParamID::chorusMix);
    setupKnob (delayMix,      "Delay",        ParamID::delayMix);
    setupKnob (delayFb,       "Feedback",     ParamID::delayFb);
    setupKnob (delayTimeMs,   "Div",          ParamID::delayDiv);
    {
        static const juce::StringArray divs { "1/32", "1/16", "1/16.", "1/8", "1/8.", "1/4", "1/4." };
        delayTimeMs.knob.textFromValueFunction = [] (double v) -> juce::String
        {
            static const juce::StringArray names { "1/32", "1/16", "1/16.", "1/8", "1/8.", "1/4", "1/4." };
            return names[juce::jlimit (0, names.size() - 1, (int) std::round (v))];
        };
        delayTimeMs.knob.updateText();
    }
    setupKnob (delayTone,     "Tone",         ParamID::delayTone);

    setupKnob (reverbMix,     "Reverb",       ParamID::reverbMix);
    setupKnob (reverbSize,    "Size",         ParamID::reverbSize);
    setupKnob (reverbMod,     "Mod",          ParamID::reverbMod);
    setupKnob (shimmerVol,    "Shimmer",      ParamID::shimmerVol);

    // Reverb size readout: 1 → 10 s (matches the standalone's display).
    reverbSize.knob.textFromValueFunction = [] (double v) -> juce::String
    {
        return juce::String ((int) std::round (1.0 + v * 9.0)) + "s";
    };
    reverbSize.knob.updateText();

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

    setupKnob (eqLow,         "Low",          ParamID::eqLow,    " dB");
    setupKnob (eqLoMid,       "Lo-Mid",       ParamID::eqLoMid,  " dB");
    setupKnob (eqHiMid,       "Hi-Mid",       ParamID::eqHiMid,  " dB");
    setupKnob (eqHigh,        "High",         ParamID::eqHigh,   " dB");

    setupKnob (widthMod,      "Width",        ParamID::widthMod);
    setupKnob (satAmt,        "Saturation",   ParamID::satAmt);
    setupKnob (masterVol,     "Master",       ParamID::masterVol);

    setupKnob (arpVol,        "Arp",          ParamID::arpVol);
    setupKnob (arpRate,       "Rate",         ParamID::arpRate);
    {
        static const juce::StringArray rateChoices { "1/16", "1/8", "1/4", "1/2", "1 bar" };
        arpRate.knob.textFromValueFunction = [] (double v) -> juce::String
        {
            const int idx = juce::jlimit (0, rateChoices.size() - 1, (int) std::round (v));
            return rateChoices[idx];
        };
        arpRate.knob.updateText();
    }
    // Arp octaves as a 3-pill button row instead of a knob (it's a 3-way choice).
    arpOctavesRow = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::arpOctaves,
        juce::Colour (0xff7eb6d4));
    addAndMakeVisible (*arpOctavesRow);

    setupKnob (drumVol,       "Movement",     ParamID::drumVol);

    // Voice / Pattern pickers as pill button rows (knobs are awkward
    // for short discrete choices).
    arpVoiceRow    = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::arpVoice,
        juce::Colour (0xff7eb6d4));    // arp blue
    drumPatternRow = std::make_unique<ChoiceButtonRow> (
        owner.getAPVTS(), ParamID::drumPattern,
        juce::Colour (0xffd97171));    // drum red
    addAndMakeVisible (*arpVoiceRow);
    addAndMakeVisible (*drumPatternRow);

    // ── Pads 2 fader (new alt-pad layer) ──────────────────────────────
    setupKnob (padsVol2, "Pads 2", ParamID::padsVol2);

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

    // The Size and Feedback knobs were dropped from the FX row per
    // request — keep the params alive (they still affect the audio at
    // their saved values) but hide the controls.
    for (auto* k : { &reverbSize, &delayFb, &delayTimeMs, &delayTone })
    {
        k->label.setVisible (false);
        k->knob .setVisible (false);
    }

    setSize (1080, 760);

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
                     &reverbMix, &reverbSize, &shimmerVol, &chorusMix,
                     &delayMix, &delayFb, &widthMod, &satAmt,
                     &evolveRate, &delayTimeMs, &delayTone, &reverbMod,
                     &hpfFreq, &eqLow, &eqLoMid, &eqHiMid, &eqHigh,
                     &arpRate })
        k->knob.setLookAndFeel (nullptr);
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

    // ── Mixer surface backplane (the nanoKONTROL strip) ──────────────
    if (! mixerPanelBounds.isEmpty())
    {
        const auto r = mixerPanelBounds.toFloat();
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelBg));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelEdge));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);

        // Thin separator between knobs row and faders row.
        const float kKnobsHeight = 96.0f;
        const float sepY = r.getY() + kKnobsHeight + 4.0f;
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelEdge).withAlpha (0.6f));
        g.drawHorizontalLine ((int) sepY, r.getX() + 12.0f, r.getRight() - 12.0f);
    }
}

void NorcoastAmbienceEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16);

    // ── Title strip: logo (drawn in paint), chord header, preset+stop ──
    {
        auto title = bounds.removeFromTop (52);
        title.removeFromLeft (60);

        auto right = title.removeFromRight (300).reduced (0, 10);
        loadButton.setBounds (right.removeFromRight (44));
        right.removeFromRight (4);
        saveButton.setBounds (right.removeFromRight (44));
        right.removeFromRight (6);
        stopButton.setBounds (right.removeFromRight (52));
        right.removeFromRight (8);
        presetBox.setBounds (right);

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

    // 8 columns evenly distributed across the mixer.
    auto layoutEightCols = [] (juce::Rectangle<int> r,
                                std::array<juce::Rectangle<int>, 8>& out)
    {
        const int colW = r.getWidth() / 8;
        for (int i = 0; i < 8; ++i)
            out[(size_t) i] = r.removeFromLeft (colW).reduced (3, 0);
    };

    // Top of mixer: 8 FX mix knobs (labels + rotary sliders). Layout
    // mirrors the fader column directly below where it makes sense:
    //   pos 7 (HPF)   sits above LPF — paired filter sweep.
    //   pos 8 (Mod)   sits above Master — verb-mod is the most-tweaked
    //                                     "character" knob during gigs.
    {
        auto knobsArea = mixerInner.removeFromTop (kKnobsRowHeight);
        std::array<juce::Rectangle<int>, 8> cols;
        layoutEightCols (knobsArea, cols);

        ParamKnob* fxKnobs[8] = {
            &reverbMix, &shimmerVol, &chorusMix, &delayMix,
            &widthMod,  &satAmt,     &hpfFreq,   &reverbMod
        };
        for (int i = 0; i < 8; ++i)
        {
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

        for (int i = 0; i < 8; ++i)
        {
            auto col = cols[(size_t) i];
            strips[i].fader->label.setBounds (col.removeFromTop (kKnobLabelH));

            // Mute pill across the top of the fader column. Sits under
            // the label, just like the web app's mute dot above the
            // slider.
            const int muteH = 18;
            auto muteRow = col.removeFromTop (muteH);
            if (strips[i].mute != nullptr)
                strips[i].mute->setBounds (muteRow.reduced (10, 1));
            col.removeFromTop (4);

            // The fader fills the rest of the column.
            strips[i].fader->knob.setBounds (col.reduced (8, 2));
        }
    }

    // ── Top half (everything above the mixer slab) ──────────────────
    bounds.removeFromBottom (10);

    // Row: chord-pool pills · drone · evolve · evolve bars knob.
    {
        auto evolveRow = bounds.removeFromTop (32);
        if (chordPoolRow != nullptr)
            chordPoolRow->setBounds (evolveRow.removeFromLeft (evolveRow.getWidth() * 5 / 9).reduced (2, 2));
        evolveRow.removeFromLeft (8);

        auto barsLabel = evolveRow.removeFromLeft (40);
        evolveRate.label.setBounds (barsLabel);
        evolveRate.knob.setBounds (evolveRow.removeFromLeft (60).reduced (0, 2));
        evolveRow.removeFromLeft (8);
        droneButton .setBounds (evolveRow.removeFromLeft (60).reduced (2, 4));
        evolveRow.removeFromLeft (4);
        evolveButton.setBounds (evolveRow.removeFromLeft (60).reduced (2, 4));
    }
    bounds.removeFromTop (4);

    // Row: custom-chord degree pills.
    {
        auto degreesRow = bounds.removeFromTop (28);
        if (customDegreesRow != nullptr)
            customDegreesRow->setBounds (degreesRow);
    }
    bounds.removeFromTop (4);

    // Row: 12-key root grid (full width).
    {
        auto keyRow = bounds.removeFromTop (44);
        if (rootKeyRow != nullptr) rootKeyRow->setBounds (keyRow);
    }
    bounds.removeFromTop (6);

    // Row: drum pattern pills · arp pills (rate as knob, voice + octaves) · sub/tex toggles · EQ.
    {
        auto controlsRow = bounds.removeFromTop (28);
        if (drumPatternRow != nullptr)
            drumPatternRow->setBounds (controlsRow.removeFromLeft (controlsRow.getWidth() * 6 / 13).reduced (2, 0));
        controlsRow.removeFromLeft (6);
        if (arpVoiceRow != nullptr)
            arpVoiceRow->setBounds (controlsRow.removeFromLeft (controlsRow.getWidth() / 3).reduced (2, 0));
        controlsRow.removeFromLeft (4);
        if (arpOctavesRow != nullptr)
            arpOctavesRow->setBounds (controlsRow.removeFromLeft (controlsRow.getWidth() / 2).reduced (2, 0));
        controlsRow.removeFromLeft (4);
        subOctButton    .setBounds (controlsRow.removeFromLeft (controlsRow.getWidth() / 3).reduced (2, 0));
        controlsRow.removeFromLeft (2);
        textureOctButton.setBounds (controlsRow.removeFromLeft (controlsRow.getWidth() / 2).reduced (2, 0));
        controlsRow.removeFromLeft (2);
        eqToggleButton  .setBounds (controlsRow.reduced (2, 0));
    }
    bounds.removeFromTop (4);

    // Row: arp rate knob + (when expanded) EQ knobs.
    // HPF moved down into the mixer surface (above the LPF fader).
    {
        auto knobsRow = bounds.removeFromTop (76);
        const int colW = eqExpanded ? knobsRow.getWidth() / 5 : knobsRow.getWidth();

        auto arpCol = knobsRow.removeFromLeft (colW);
        arpRate.label.setBounds (arpCol.removeFromTop (kKnobLabelH));
        arpRate.knob .setBounds (arpCol.reduced (4, 2));

        if (eqExpanded)
        {
            ParamKnob* eqs[4] = { &eqLow, &eqLoMid, &eqHiMid, &eqHigh };
            for (auto* eq : eqs)
            {
                auto col = knobsRow.removeFromLeft (colW);
                eq->label.setBounds (col.removeFromTop (kKnobLabelH));
                eq->knob .setBounds (col.reduced (4, 2));
            }
        }
    }
    bounds.removeFromTop (6);

    // Row: drum step sequencer (16 × 3).
    if (stepSequencer != nullptr)
    {
        const int seqH = juce::jmin (78, bounds.getHeight());
        stepSequencer->setBounds (bounds.removeFromTop (seqH));
    }

    // Park the invisible qwerty-keyboard 1×1 in the bottom-right corner.
    qwertyKeyboard.setBounds (getWidth() - 2, getHeight() - 2, 1, 1);
}
