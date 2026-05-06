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
    if (suffix.isNotEmpty())
        k.knob.setTextValueSuffix (suffix);
    addAndMakeVisible (k.knob);

    k.attach = std::make_unique<SliderAttach> (owner.getAPVTS(), paramID, k.knob);

    // Double-click resets the knob to its parameter default.
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
                      juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&laf);

    // Invisible focus-catcher for computer-keyboard input. JUCE's
    // built-in qwerty mapping (a/w/s/e/d/f/t/g/y/h/u/j/k = C..C+1)
    // does the work; we just need a focusable child component.
    addAndMakeVisible (qwertyKeyboard);
    qwertyKeyboard.setKeyPressBaseOctave (4);
    qwertyKeyboard.setWantsKeyboardFocus (true);
    qwertyKeyboard.setVelocity (0.7f, false);

    addAndMakeVisible (subOctButton);
    subOctButton.setClickingTogglesState (true);
    subOctAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::foundationSubOct, subOctButton);

    addAndMakeVisible (padsOctButton);
    padsOctButton.setClickingTogglesState (true);
    padsOctAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::padsOctUp, padsOctButton);

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
    presetBox.setText ("Preset…", juce::dontSendNotification);
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

    setupKnob (chordType,     "Chord",        ParamID::chordType);
    setupKnob (evolveRate,    "Rate",         ParamID::evolveRate, " s");
    setupKnob (homeRoot,      "Root",         ParamID::homeRoot);

    {
        const auto names = ChordEvolver::getChordNames();
        chordType.knob.textFromValueFunction = [names] (double v) -> juce::String
        {
            const int idx = juce::jlimit (0, names.size() - 1, (int) std::round (v));
            return names[idx];
        };
        chordType.knob.updateText();
    }
    {
        static const juce::StringArray noteNames
            { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        homeRoot.knob.textFromValueFunction = [] (double v) -> juce::String
        {
            const int idx = juce::jlimit (0, noteNames.size() - 1, (int) std::round (v));
            return noteNames[idx];
        };
        homeRoot.knob.updateText();
    }

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
    setupKnob (arpRate,       "Rate",         ParamID::arpRate, " b");
    setupKnob (arpOctaves,    "Octaves",      ParamID::arpOctaves);
    setupKnob (arpVoice,      "Voice",        ParamID::arpVoice);

    setupKnob (drumVol,       "Vol",          ParamID::drumVol);
    setupKnob (drumPattern,   "Pattern",      ParamID::drumPattern);

    sections =
    {{
        { "LAYERS",     kColLayers,   {}, { &foundationVol, &padsVol, &textureVol } },
        { "EVOLVE",     kColEvolve,   {}, { &homeRoot, &chordType, &evolveRate } },
        { "CHORUS",     kColModFx,    {}, { &chorusMix } },
        { "DELAY",      kColModFx,    {}, { &delayMix, &delayFb, &delayTimeMs, &delayTone } },
        { "REVERB",     kColReverbFx, {}, { &reverbMix, &reverbSize, &reverbMod, &shimmerVol } },
        { "FILTER",     kColFilter,   {}, { &hpfFreq, &lpfFreq } },
        { "EQ",         kColEq,       {}, { &eqLow, &eqLoMid, &eqHiMid, &eqHigh } },
        { "ARP",        kColArp,      {}, { &arpVol, &arpRate, &arpOctaves, &arpVoice } },
        { "DRUMS",      kColDrums,    {}, { &drumVol, &drumPattern } }
    }};

    setSize (1080, 480);

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

    auto bounds = getLocalBounds().reduced (16);

    // Just the logo. No subtitle, no version. Clean.
    auto top = bounds.removeFromTop (40);
    g.setColour (juce::Colour (NorcoastLookAndFeel::kAccent));
    g.setFont (juce::FontOptions (20.0f).withStyle ("Light"));
    g.drawText ("NORCOAST AMBIENCE",
                top.withTrimmedLeft (8),
                juce::Justification::centredLeft);

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

    // Preset bar — small, right-aligned in the title strip.
    {
        auto title = bounds.removeFromTop (40);
        auto right = title.removeFromRight (220).reduced (0, 8);
        loadButton.setBounds (right.removeFromRight (44));
        right.removeFromRight (4);
        saveButton.setBounds (right.removeFromRight (44));
        right.removeFromRight (6);
        presetBox.setBounds  (right);
    }
    bounds.removeFromTop (8);

    // 9 panels in a 5+4 grid.
    auto knobArea = bounds.removeFromTop (340);
    const int rowH = (knobArea.getHeight() - 8) / 2;
    auto row1 = knobArea.removeFromTop (rowH);
    knobArea.removeFromTop (8);
    auto row2 = knobArea;

    // Top: LAYERS (3) · EVOLVE (3) · CHORUS (1) · DELAY (4) · REVERB (4) → 15 col-units
    {
        const int w = row1.getWidth();
        const int colA = (int)(w * (3.0f / 15.0f));
        const int colB = (int)(w * (3.0f / 15.0f));
        const int colC = (int)(w * (1.0f / 15.0f));
        const int colD = (int)(w * (4.0f / 15.0f));
        sections[0].bounds = row1.removeFromLeft (colA).reduced (4, 0);
        sections[1].bounds = row1.removeFromLeft (colB).reduced (4, 0);
        sections[2].bounds = row1.removeFromLeft (colC).reduced (4, 0);
        sections[3].bounds = row1.removeFromLeft (colD).reduced (4, 0);
        sections[4].bounds = row1.reduced (4, 0);
    }

    // Bottom: FILTER (2) · EQ (4) · ARP (4) · DRUMS (2) → 12 col-units
    {
        const int w = row2.getWidth();
        const int colA = (int)(w * (2.0f / 12.0f));
        const int colB = (int)(w * (4.0f / 12.0f));
        const int colC = (int)(w * (4.0f / 12.0f));
        sections[5].bounds = row2.removeFromLeft (colA).reduced (4, 0);
        sections[6].bounds = row2.removeFromLeft (colB).reduced (4, 0);
        sections[7].bounds = row2.removeFromLeft (colC).reduced (4, 0);
        sections[8].bounds = row2.reduced (4, 0);
    }

    // Knobs + per-section toggle button row (LAYERS / EVOLVE).
    for (auto& s : sections)
    {
        if (s.knobs.empty()) continue;
        auto inner = s.bounds.reduced (kSectionPadX, kSectionPadY)
                             .withTrimmedTop (kSectionHeaderH);

        if (&s == &sections[0])           // LAYERS — 3 toggles
        {
            auto row = inner.removeFromBottom (24).reduced (4, 0);
            const int third = row.getWidth() / 3;
            subOctButton    .setBounds (row.removeFromLeft (third).reduced (2, 0));
            padsOctButton   .setBounds (row.removeFromLeft (third).reduced (2, 0));
            textureOctButton.setBounds (row.reduced (2, 0));
        }
        else if (&s == &sections[1])      // EVOLVE — Drone + Evolve toggles
        {
            auto row = inner.removeFromBottom (24).reduced (4, 0);
            const int half = row.getWidth() / 2;
            droneButton .setBounds (row.removeFromLeft (half).reduced (2, 0));
            evolveButton.setBounds (row.reduced (2, 0));
        }

        const int kW = inner.getWidth() / (int) s.knobs.size();
        for (auto* k : s.knobs)
        {
            auto col = inner.removeFromLeft (kW);
            k->label.setBounds (col.removeFromTop (kKnobLabelH));
            k->knob .setBounds (col.reduced (4, 2));
        }
    }

    // Park the invisible qwerty-keyboard 1×1 in the bottom-right corner.
    qwertyKeyboard.setBounds (getWidth() - 2, getHeight() - 2, 1, 1);
}
