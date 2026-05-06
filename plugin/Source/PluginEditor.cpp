#include "PluginEditor.h"
#include "Parameters.h"
#include "Presets.h"

namespace
{
    constexpr int kKnobW       = 78;
    constexpr int kKnobH       = 78;
    constexpr int kKnobLabelH  = 16;
    constexpr int kSectionPadX = 12;
    constexpr int kSectionPadY = 8;
    constexpr int kSectionHeaderH = 20;

    // Distinct accent hue per section so sections stay visually grouped.
    // Pulled from the standalone's slider strip palette.
    const juce::Colour kColLayers   { 0xff5eb88a }; // Foundation green
    const juce::Colour kColModFx    { 0xffc4b8e8 }; // chorus lavender
    const juce::Colour kColReverbFx { 0xffc4915e }; // reverb warm orange (signature)
    const juce::Colour kColFilter   { 0xffd46b8a }; // filter pink
    const juce::Colour kColEq       { 0xffb07acc }; // EQ purple
    const juce::Colour kColMaster   { 0xffe0d2a8 }; // master cream

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
}

NorcoastAmbienceEditor::NorcoastAmbienceEditor (NorcoastAmbienceProcessor& p)
    : juce::AudioProcessorEditor (&p),
      owner    (p),
      keyboard (p.getKeyboardState())
{
    setLookAndFeel (&laf);

    addAndMakeVisible (keyboard);
    keyboard.setLowestVisibleKey (48);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId,
                        juce::Colour::fromRGB (0xc4, 0xc0, 0xb6));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId,
                        juce::Colour (NorcoastLookAndFeel::kBgEdge));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
                        juce::Colour (NorcoastLookAndFeel::kAccent).withAlpha (0.55f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                        juce::Colour (NorcoastLookAndFeel::kAccent).withAlpha (0.25f));

    addAndMakeVisible (latchButton);
    latchButton.setClickingTogglesState (true);
    latchButton.onClick = [this] { keyboard.setLatched (latchButton.getToggleState()); };

    addAndMakeVisible (allOffButton);
    allOffButton.onClick = [this] { owner.getKeyboardState().allNotesOff (1); };

    // Live oscilloscope of the master output — sits between the buttons
    // row and the keyboard.
    addAndMakeVisible (owner.getOscilloscope());

    addAndMakeVisible (subOctButton);
    subOctButton.setClickingTogglesState (true);
    subOctAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::foundationSubOct, subOctButton);

    addAndMakeVisible (textureOctButton);
    textureOctButton.setClickingTogglesState (true);
    textureOctAttach = std::make_unique<ButtonAttach> (
        owner.getAPVTS(), ParamID::textureOctUp, textureOctButton);

    // ── Preset bar ──────────────────────────────────────────────────────
    addAndMakeVisible (presetBox);
    presetBox.setColour (juce::ComboBox::backgroundColourId,
                         juce::Colour (NorcoastLookAndFeel::kPanelBg));
    presetBox.setColour (juce::ComboBox::textColourId,
                         juce::Colour (NorcoastLookAndFeel::kAccent));
    presetBox.setColour (juce::ComboBox::arrowColourId,
                         juce::Colour (NorcoastLookAndFeel::kAccent));
    int id = 1;
    for (const auto& p : Presets::factory())
        presetBox.addItem (p.name, id++);
    presetBox.setText ("Preset…", juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0)
            applyFactoryPreset (idx);
    };

    addAndMakeVisible (saveButton);
    saveButton.onClick = [this] { savePresetToFile(); };
    addAndMakeVisible (loadButton);
    loadButton.onClick = [this] { loadPresetFromFile(); };

    setupKnob (foundationVol, "Foundation",   ParamID::foundationVol);
    setupKnob (padsVol,       "Pads",         ParamID::padsVol);
    setupKnob (textureVol,    "Texture",      ParamID::textureVol);

    setupKnob (chorusMix,     "Chorus",       ParamID::chorusMix);
    setupKnob (delayMix,      "Delay",        ParamID::delayMix);
    setupKnob (delayFb,       "Feedback",     ParamID::delayFb);
    setupKnob (delayTimeMs,   "Time",         ParamID::delayTimeMs, " ms");
    setupKnob (delayTone,     "Tone",         ParamID::delayTone);

    setupKnob (reverbMix,     "Reverb",       ParamID::reverbMix);
    setupKnob (reverbSize,    "Size",         ParamID::reverbSize);
    setupKnob (reverbMod,     "Mod",          ParamID::reverbMod);
    setupKnob (shimmerVol,    "Shimmer",      ParamID::shimmerVol);

    setupKnob (hpfFreq,       "High Pass",    ParamID::hpfFreq);
    setupKnob (lpfFreq,       "Low Pass",     ParamID::lpfFreq);

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

    setupKnob (velocitySens,  "Vel Sens",     ParamID::velocitySens);
    setupKnob (pitchBendRange, "PB Range",    ParamID::pitchBendRange, " st");

    // Per-section accent colour and content. Layout in resized() arranges
    // them into a 4×2 grid (top row 4 sections, bottom row 3 sections).
    const juce::Colour kColArp   { 0xff7eb6d4 };  // arp blue
    const juce::Colour kColDrums { 0xffd97171 };  // drum red
    sections =
    {{
        { "LAYERS",     kColLayers,   {}, { &foundationVol, &padsVol, &textureVol } },
        { "CHORUS",     kColModFx,    {}, { &chorusMix } },
        { "DELAY",      kColModFx,    {}, { &delayMix, &delayFb, &delayTimeMs, &delayTone } },
        { "REVERB",     kColReverbFx, {}, { &reverbMix, &reverbSize, &reverbMod, &shimmerVol } },
        { "FILTER+EQ",  kColEq,       {}, { &hpfFreq, &lpfFreq, &eqLow, &eqLoMid, &eqHiMid, &eqHigh } },
        { "ARP",        kColArp,      {}, { &arpVol, &arpRate, &arpOctaves, &arpVoice } },
        { "DRUMS",      kColDrums,    {}, { &drumVol, &drumPattern } },
        { "MASTER",     kColMaster,   {}, { &velocitySens, &pitchBendRange,
                                              &widthMod, &satAmt, &masterVol } }
    }};

    setSize (1020, 680);
}

NorcoastAmbienceEditor::~NorcoastAmbienceEditor()
{
    setLookAndFeel (nullptr);
    auto clearLF = [this] (ParamKnob& k) { k.knob.setLookAndFeel (nullptr); };
    for (auto& s : sections)
        for (auto* k : s.knobs)
            clearLF (*k);

    // Oscilloscope is owned by the processor; detach so it isn't deleted
    // by the editor's child-component teardown.
    removeChildComponent (&owner.getOscilloscope());
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
    // Background
    juce::ColourGradient bg (juce::Colour (NorcoastLookAndFeel::kBg), 0.0f, 0.0f,
                             juce::Colour (NorcoastLookAndFeel::kBgEdge),
                             (float) getWidth(), (float) getHeight(), false);
    bg.addColour (0.45, juce::Colour (NorcoastLookAndFeel::kBgMid));
    g.setGradientFill (bg);
    g.fillAll();

    auto bounds = getLocalBounds().reduced (16);

    // Title
    auto top = bounds.removeFromTop (52);
    g.setColour (juce::Colour (NorcoastLookAndFeel::kAccent));
    g.setFont (juce::FontOptions (24.0f).withStyle ("Light"));
    g.drawText ("NORCOAST AMBIENCE",
                top.removeFromTop (32).withTrimmedLeft (8),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (NorcoastLookAndFeel::kTextDim));
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("ambient synth · v1.8 · phase 14 · texture · presets · scope",
                top.withTrimmedLeft (8),
                juce::Justification::centredLeft);

    // Section panels
    for (auto& s : sections)
    {
        const auto r = s.bounds.toFloat();
        if (r.isEmpty()) continue;

        // Panel body
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelBg));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelEdge));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        // Header strip — a thin coloured line at top
        const auto headerStrip = r.withHeight (3.0f).reduced (10.0f, 0.0f);
        g.setColour (s.accent.withAlpha (0.85f));
        g.fillRoundedRectangle (headerStrip, 1.5f);

        // Header title
        g.setColour (s.accent);
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        g.drawText (s.title,
                    s.bounds.withTrimmedTop (8).withHeight (kSectionHeaderH).reduced (10, 0),
                    juce::Justification::centredLeft);
    }

    // Footer
    g.setColour (juce::Colour (0x55ffffff));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("plugin · v1.7 · phase 12  (texture · drums · arp · sub-oct · rotary GUI)",
                getLocalBounds().removeFromBottom (24).reduced (16, 4),
                juce::Justification::centredRight);
}

void NorcoastAmbienceEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16);

    // Title strip — preset controls live on the right side.
    {
        auto title = bounds.removeFromTop (52);
        auto right = title.removeFromRight (340).reduced (0, 8);
        loadButton.setBounds  (right.removeFromRight (60));
        right.removeFromRight (4);
        saveButton.setBounds  (right.removeFromRight (60));
        right.removeFromRight (8);
        presetBox.setBounds   (right.reduced (0, 2));
    }
    bounds.removeFromTop (8);

    // Knob area: 2 rows of section panels.
    auto knobArea = bounds.removeFromTop (340);
    const int rowH = (knobArea.getHeight() - 8) / 2;
    auto row1 = knobArea.removeFromTop (rowH);
    knobArea.removeFromTop (8);
    auto row2 = knobArea;

    // Top row: LAYERS (3), CHORUS (1), DELAY (4), REVERB (4) → 12 cols
    {
        const int row1Width = row1.getWidth();
        const int colA = (int)(row1Width * (3.0f / 12.0f));
        const int colB = (int)(row1Width * (1.0f / 12.0f));
        const int colC = (int)(row1Width * (4.0f / 12.0f));
        sections[0].bounds = row1.removeFromLeft (colA).reduced (4, 0);
        sections[1].bounds = row1.removeFromLeft (colB).reduced (4, 0);
        sections[2].bounds = row1.removeFromLeft (colC).reduced (4, 0);
        sections[3].bounds = row1.reduced (4, 0);
    }

    // Bottom row: FILTER+EQ (6), ARP (4), DRUMS (2), MASTER (5) → 17 cols
    {
        const int row2Width = row2.getWidth();
        const int colA = (int)(row2Width * (6.0f / 17.0f));
        const int colB = (int)(row2Width * (4.0f / 17.0f));
        const int colC = (int)(row2Width * (2.0f / 17.0f));
        sections[4].bounds = row2.removeFromLeft (colA).reduced (4, 0);
        sections[5].bounds = row2.removeFromLeft (colB).reduced (4, 0);
        sections[6].bounds = row2.removeFromLeft (colC).reduced (4, 0);
        sections[7].bounds = row2.reduced (4, 0);
    }

    // Lay out knobs inside each section
    for (auto& s : sections)
    {
        if (s.knobs.empty()) continue;
        auto inner = s.bounds.reduced (kSectionPadX, kSectionPadY)
                             .withTrimmedTop (kSectionHeaderH);

        // LAYERS section also gets two toggle buttons docked at the bottom.
        const bool isLayers = (&s == &sections[0]);
        if (isLayers)
        {
            auto buttonArea = inner.removeFromBottom (24).reduced (4, 0);
            const int half = buttonArea.getWidth() / 2;
            subOctButton    .setBounds (buttonArea.removeFromLeft (half).reduced (2, 0));
            textureOctButton.setBounds (buttonArea.reduced (2, 0));
        }

        const int kW = inner.getWidth() / (int) s.knobs.size();
        for (auto* k : s.knobs)
        {
            auto col = inner.removeFromLeft (kW);
            k->label.setBounds (col.removeFromTop (kKnobLabelH));
            k->knob .setBounds (col.reduced (4, 2));
        }
    }

    bounds.removeFromTop (12);

    // Buttons row
    auto buttons = bounds.removeFromTop (32);
    const int buttonW = 96;
    latchButton.setBounds  (buttons.removeFromLeft (buttonW));
    buttons.removeFromLeft (8);
    allOffButton.setBounds (buttons.removeFromLeft (buttonW));

    // Place oscilloscope on the right side of the button row strip,
    // filling the available width.
    auto scopeStrip = bounds.removeFromTop (44);
    owner.getOscilloscope().setBounds (scopeStrip.reduced (4, 4));

    // Keyboard
    auto kbStrip = getLocalBounds().removeFromBottom (124).reduced (16, 8);
    keyboard.setBounds (kbStrip);
}
