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
    const juce::Colour kColLayers   { 0xff5eb88a }; // green
    const juce::Colour kColModFx    { 0xffc4b8e8 }; // lavender
    const juce::Colour kColReverbFx { 0xffc4915e }; // warm orange
    const juce::Colour kColFilter   { 0xffd46b8a }; // pink
    const juce::Colour kColEq       { 0xffb07acc }; // purple
    const juce::Colour kColArp      { 0xff7eb6d4 }; // blue
    const juce::Colour kColDrums    { 0xffd97171 }; // red
    const juce::Colour kColMaster   { 0xffe0d2a8 }; // cream
    const juce::Colour kColEvolve   { 0xffaadc8a }; // lime — chord evolve

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
    setupKnob (lpfFreq,       "Low Pass",     ParamID::lpfFreq);

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

    setupKnob (arpVol,        "Vol",          ParamID::arpVol);
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

    setupKnob (drumVol,       "Vol",          ParamID::drumVol);

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

    sections =
    {{
        { "LAYERS",     kColLayers,   {}, { &foundationVol, &padsVol, &textureVol } },
        { "EVOLVE",     kColEvolve,   {}, { &evolveRate } },
        { "CHORUS",     kColModFx,    {}, { &chorusMix } },
        { "DELAY",      kColModFx,    {}, { &delayMix, &delayFb, &delayTimeMs, &delayTone } },
        { "REVERB",     kColReverbFx, {}, { &reverbMix, &reverbSize, &reverbMod, &shimmerVol } },
        { "FILTER",     kColFilter,   {}, { &hpfFreq, &lpfFreq } },
        { "EQ",         kColEq,       {}, { &eqLow, &eqLoMid, &eqHiMid, &eqHigh } },
        { "ARP",        kColArp,      {}, { &arpVol, &arpRate } },
        { "DRUMS",      kColDrums,    {}, { &drumVol } },
        { "MASTER",     kColMaster,   {}, { &widthMod, &satAmt, &masterVol } }
    }};

    setSize (980, 730);

    // Give the qwerty keyboard focus so computer-keyboard keys map to MIDI.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (&qwertyKeyboard)]
    {
        if (safeThis != nullptr) safeThis->grabKeyboardFocus();
    });
}

NorcoastAmbienceEditor::~NorcoastAmbienceEditor()
{
    setLookAndFeel (nullptr);
    auto clearLF = [] (ParamKnob& k) { k.knob.setLookAndFeel (nullptr); };
    for (auto& s : sections)
        for (auto* k : s.knobs)
            clearLF (*k);
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

    // Logo: small accent-orange circle with a sine wave arc through it,
    // top-left. Drawn here rather than as a Component for simplicity.
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

    for (auto& s : sections)
    {
        const auto r = s.bounds.toFloat();
        if (r.isEmpty()) continue;

        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelBg));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelEdge));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        const auto headerStrip = r.withHeight (3.0f).reduced (10.0f, 0.0f);
        g.setColour (s.accent.withAlpha (0.85f));
        g.fillRoundedRectangle (headerStrip, 1.5f);

        g.setColour (s.accent);
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        g.drawText (s.title,
                    s.bounds.withTrimmedTop (8).withHeight (kSectionHeaderH).reduced (10, 0),
                    juce::Justification::centredLeft);
    }

}

void NorcoastAmbienceEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16);

    // Title strip — logo (drawn in paint), chord state header centred,
    // preset bar + Stop on the right.
    {
        auto title = bounds.removeFromTop (52);
        title.removeFromLeft (60);                  // leave room for logo

        auto right = title.removeFromRight (300).reduced (0, 10);
        loadButton.setBounds (right.removeFromRight (44));
        right.removeFromRight (4);
        saveButton.setBounds (right.removeFromRight (44));
        right.removeFromRight (6);
        stopButton.setBounds (right.removeFromRight (52));
        right.removeFromRight (8);
        presetBox.setBounds  (right);

        chordHeader.setBounds (title);
    }
    bounds.removeFromTop (8);

    // 10 panels in a 5+5 grid.
    auto knobArea = bounds.removeFromTop (440);
    const int rowH = (knobArea.getHeight() - 8) / 2;
    auto row1 = knobArea.removeFromTop (rowH);
    knobArea.removeFromTop (8);
    auto row2 = knobArea;

    // Top: LAYERS (3) · EVOLVE (4) · CHORUS (2) · DELAY (4) · REVERB (4) → 17 units
    // EVOLVE bumped 3→4 so the 6 chord-pool pills fit; CHORUS 1→2.
    {
        const int w = row1.getWidth();
        const int colA = (int)(w * (3.0f / 17.0f));
        const int colB = (int)(w * (4.0f / 17.0f));
        const int colC = (int)(w * (2.0f / 17.0f));
        const int colD = (int)(w * (4.0f / 17.0f));
        sections[0].bounds = row1.removeFromLeft (colA).reduced (4, 0);
        sections[1].bounds = row1.removeFromLeft (colB).reduced (4, 0);
        sections[2].bounds = row1.removeFromLeft (colC).reduced (4, 0);
        sections[3].bounds = row1.removeFromLeft (colD).reduced (4, 0);
        sections[4].bounds = row1.reduced (4, 0);
    }

    // Bottom: FILTER (2) · EQ (4) · ARP (3) · DRUMS (3) · MASTER (3) → 15 units
    // DRUMS bumped 2→3 so its 6 pattern pills fit.
    {
        const int w = row2.getWidth();
        const int colA = (int)(w * (2.0f / 15.0f));
        const int colB = (int)(w * (4.0f / 15.0f));
        const int colC = (int)(w * (3.0f / 15.0f));
        const int colD = (int)(w * (3.0f / 15.0f));
        sections[5].bounds = row2.removeFromLeft (colA).reduced (4, 0);
        sections[6].bounds = row2.removeFromLeft (colB).reduced (4, 0);
        sections[7].bounds = row2.removeFromLeft (colC).reduced (4, 0);
        sections[8].bounds = row2.removeFromLeft (colD).reduced (4, 0);
        sections[9].bounds = row2.reduced (4, 0);
    }

    // Knobs + per-section toggle button row (LAYERS / EVOLVE).
    for (auto& s : sections)
    {
        if (s.knobs.empty()) continue;
        auto inner = s.bounds.reduced (kSectionPadX, kSectionPadY)
                             .withTrimmedTop (kSectionHeaderH);

        if (&s == &sections[0])           // LAYERS — 2 toggles
        {
            auto row = inner.removeFromBottom (24).reduced (4, 0);
            const int half = row.getWidth() / 2;
            subOctButton    .setBounds (row.removeFromLeft (half).reduced (2, 0));
            textureOctButton.setBounds (row.reduced (2, 0));
        }
        else if (&s == &sections[7] && arpVoiceRow && arpOctavesRow)  // ARP
        {
            auto voiceRow = inner.removeFromBottom (22).reduced (4, 0);
            arpVoiceRow->setBounds (voiceRow);
            inner.removeFromBottom (3);
            auto octRow   = inner.removeFromBottom (22).reduced (4, 0);
            arpOctavesRow->setBounds (octRow);
        }
        else if (&s == &sections[8] && drumPatternRow)     // DRUMS — pattern pills
        {
            auto row = inner.removeFromBottom (24).reduced (4, 0);
            drumPatternRow->setBounds (row);
        }
        else if (&s == &sections[1])      // EVOLVE — pill row, knob, then toggles
        {
            // Chord type pill row at the top of the content area.
            auto pillRow = inner.removeFromTop (28).reduced (2, 0);
            if (chordPoolRow != nullptr) chordPoolRow->setBounds (pillRow);
            inner.removeFromTop (4);

            // Drone + Evolve toggles docked at the bottom.
            auto buttonRow = inner.removeFromBottom (24).reduced (4, 0);
            const int half = buttonRow.getWidth() / 2;
            droneButton .setBounds (buttonRow.removeFromLeft (half).reduced (2, 0));
            evolveButton.setBounds (buttonRow.reduced (2, 0));
        }

        const int kW = inner.getWidth() / (int) s.knobs.size();
        for (auto* k : s.knobs)
        {
            auto col = inner.removeFromLeft (kW);
            k->label.setBounds (col.removeFromTop (kKnobLabelH));
            k->knob .setBounds (col.reduced (4, 2));
        }
    }

    // ─── Bottom rows: custom-chord degrees + 12-key root grid ────────
    bounds.removeFromTop (8);

    auto degreesRow = bounds.removeFromTop (28);
    if (customDegreesRow != nullptr) customDegreesRow->setBounds (degreesRow);
    bounds.removeFromTop (4);

    auto keyRow = bounds.removeFromTop (44);
    if (rootKeyRow != nullptr) rootKeyRow->setBounds (keyRow);

    bounds.removeFromTop (8);
    auto seqRow = bounds.removeFromTop (78);
    if (stepSequencer != nullptr) stepSequencer->setBounds (seqRow);

    // Park the invisible qwerty-keyboard 1×1 in the bottom-right corner.
    qwertyKeyboard.setBounds (getWidth() - 2, getHeight() - 2, 1, 1);
}
