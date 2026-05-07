#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PadSound.h"
#include "PadVoice.h"
#include "Parameters.h"
#include "Presets.h"

namespace
{
    // Direct port of PAD_LAYERS[0..1] from the standalone (public/index.html).
    // The layerGain field is no longer read at render time (PadVoice now reads
    // a runtime APVTS atom) but is kept as documentation of the standalone's
    // DEFAULT_VOLUMES.
    LayerConfig makeFoundationConfig()
    {
        LayerConfig c;
        c.name    = "Foundation";
        c.timbres =
        {
            { WaveType::Sub,  3, 1.2f, 0.14f, 0.05f },
            { WaveType::Warm, 2, 2.0f, 0.04f, 0.08f }
        };
        c.octaves   = { -1 };
        c.fBase     = 200.0f;
        c.fMod      = 30.0f;
        c.fRate     = 0.1f;
        c.aMod      = 0.03f;
        c.aRate     = 0.12f;
        c.layerGain = 0.55f;
        return c;
    }

    LayerConfig makePadsConfig()
    {
        LayerConfig c;
        c.name    = "Pads";
        c.timbres =
        {
            { WaveType::Lush,      6, 10.0f, 0.030f, 0.60f },
            { WaveType::Warm,      4,  7.0f, 0.022f, 0.40f },
            { WaveType::Choir,     3, 14.0f, 0.014f, 0.75f },
            { WaveType::Celestial, 2, 18.0f, 0.012f, 0.90f }
        };
        c.octaves   = { 0, 1 };
        c.fBase     = 1200.0f;
        c.fMod      = 450.0f;
        c.fRate     = 0.042f;
        c.aMod      = 0.10f;
        c.aRate     = 0.08f;
        c.layerGain = 0.50f;
        return c;
    }

    LayerConfig makePadsConfig2()
    {
        // Bright / glassy alt-pad layer — sits an octave above Pads 1 so
        // a player can blend it in for sparkle without crowding the main
        // pad's harmonic content.
        LayerConfig c;
        c.name    = "Pads 2";
        c.timbres =
        {
            { WaveType::Glass,     4,  9.0f, 0.018f, 0.70f },
            { WaveType::Celestial, 3, 12.0f, 0.010f, 0.85f },
            { WaveType::Choir,     2, 15.0f, 0.008f, 0.90f }
        };
        c.octaves   = { 1, 2 };
        c.fBase     = 2400.0f;
        c.fMod      = 600.0f;
        c.fRate     = 0.055f;
        c.aMod      = 0.08f;
        c.aRate     = 0.07f;
        c.layerGain = 0.42f;
        return c;
    }

    // reverbSizeToFB(v) = 10^(-1.5/(1+v*19))  — exact match to standalone.
    inline float reverbSizeToFB (float v) noexcept
    {
        return std::pow (10.0f, -1.5f / (1.0f + v * 19.0f));
    }
}

NorcoastAmbienceProcessor::NorcoastAmbienceProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      foundationConfig (makeFoundationConfig()),
      padsConfig       (makePadsConfig()),
      padsConfig2      (makePadsConfig2()),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    Waves::init();

    // Cache atom pointers for realtime-safe param reads.
    foundationVolParam    = apvts.getRawParameterValue (ParamID::foundationVol);
    padsVolParam          = apvts.getRawParameterValue (ParamID::padsVol);
    padsVol2Param         = apvts.getRawParameterValue (ParamID::padsVol2);
    textureVolParam       = apvts.getRawParameterValue (ParamID::textureVol);
    foundationSubOctParam = apvts.getRawParameterValue (ParamID::foundationSubOct);
    textureOctUpParam     = apvts.getRawParameterValue (ParamID::textureOctUp);
    foundationMuteParam   = apvts.getRawParameterValue (ParamID::foundationMute);
    padsMuteParam         = apvts.getRawParameterValue (ParamID::padsMute);
    pads2MuteParam        = apvts.getRawParameterValue (ParamID::pads2Mute);
    textureMuteParam      = apvts.getRawParameterValue (ParamID::textureMute);
    arpMuteParam          = apvts.getRawParameterValue (ParamID::arpMute);
    drumMuteParam         = apvts.getRawParameterValue (ParamID::drumMute);
    chorusMixParam     = apvts.getRawParameterValue (ParamID::chorusMix);
    delayMixParam      = apvts.getRawParameterValue (ParamID::delayMix);
    delayFbParam       = apvts.getRawParameterValue (ParamID::delayFb);
    delayDivParam      = apvts.getRawParameterValue (ParamID::delayDiv);
    delayToneParam     = apvts.getRawParameterValue (ParamID::delayTone);
    reverbMixParam     = apvts.getRawParameterValue (ParamID::reverbMix);
    reverbSizeParam    = apvts.getRawParameterValue (ParamID::reverbSize);
    reverbModParam     = apvts.getRawParameterValue (ParamID::reverbMod);
    lpfFreqParam       = apvts.getRawParameterValue (ParamID::lpfFreq);
    hpfFreqParam       = apvts.getRawParameterValue (ParamID::hpfFreq);
    shimmerVolParam    = apvts.getRawParameterValue (ParamID::shimmerVol);
    widthModParam      = apvts.getRawParameterValue (ParamID::widthMod);
    satAmtParam        = apvts.getRawParameterValue (ParamID::satAmt);
    masterVolParam     = apvts.getRawParameterValue (ParamID::masterVol);
    eqLowParam         = apvts.getRawParameterValue (ParamID::eqLow);
    eqLoMidParam       = apvts.getRawParameterValue (ParamID::eqLoMid);
    eqHiMidParam       = apvts.getRawParameterValue (ParamID::eqHiMid);
    eqHighParam        = apvts.getRawParameterValue (ParamID::eqHigh);
    arpVolParam        = apvts.getRawParameterValue (ParamID::arpVol);
    arpRateParam       = apvts.getRawParameterValue (ParamID::arpRate);
    arpOctavesParam    = apvts.getRawParameterValue (ParamID::arpOctaves);
    arpVoiceParam      = apvts.getRawParameterValue (ParamID::arpVoice);
    drumVolParam       = apvts.getRawParameterValue (ParamID::drumVol);
    drumPatternParam   = apvts.getRawParameterValue (ParamID::drumPattern);
    drumCustomLoParam  = apvts.getRawParameterValue (ParamID::drumCustomLo);
    drumCustomMdParam  = apvts.getRawParameterValue (ParamID::drumCustomMd);
    drumCustomHhParam  = apvts.getRawParameterValue (ParamID::drumCustomHh);
    timeSigParam       = apvts.getRawParameterValue (ParamID::timeSig);
    chordTypeParam       = apvts.getRawParameterValue (ParamID::chordType);
    customChordMaskParam   = apvts.getRawParameterValue (ParamID::customChordMask);
    enabledChordsMaskParam = apvts.getRawParameterValue (ParamID::enabledChordsMask);
    evolveOnParam      = apvts.getRawParameterValue (ParamID::evolveOn);
    evolveBarsParam    = apvts.getRawParameterValue (ParamID::evolveBars);
    droneOnParam       = apvts.getRawParameterValue (ParamID::droneOn);
    homeRootParam      = apvts.getRawParameterValue (ParamID::homeRoot);

    foundationSynth.addSound (new PadSound());
    padsSynth      .addSound (new PadSound());
    padsSynth2     .addSound (new PadSound());

    for (int i = 0; i < kVoicesPerLayer; ++i)
    {
        foundationSynth.addVoice (new PadVoice (foundationConfig, foundationVolParam,
                                                foundationSubOctParam,
                                                nullptr, nullptr,
                                                nullptr));
        padsSynth      .addVoice (new PadVoice (padsConfig, padsVolParam,
                                                nullptr,
                                                nullptr, nullptr,
                                                nullptr));
        padsSynth2     .addVoice (new PadVoice (padsConfig2, padsVol2Param,
                                                nullptr,
                                                nullptr, nullptr,
                                                nullptr));
    }
}

void NorcoastAmbienceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    foundationSynth.setCurrentPlaybackSampleRate (sampleRate);
    padsSynth      .setCurrentPlaybackSampleRate (sampleRate);
    padsSynth2     .setCurrentPlaybackSampleRate (sampleRate);

    layerScratch.setSize (2, samplesPerBlock, false, false, true);

    for (auto* m : { &muteFoundation, &mutePads, &mutePads2,
                     &muteTexture,    &muteArp,  &muteDrum })
    {
        m->reset (sampleRate, 0.05);             // 50 ms ramp
        m->setCurrentAndTargetValue (1.0f);
    }

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    juce::dsp::ProcessSpec monoSpec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    chorusDelayL.prepare (monoSpec);
    chorusDelayR.prepare (monoSpec);
    chorusDelayL.reset();
    chorusDelayR.reset();
    chorusPhaseIncL = 0.55 / sampleRate;
    chorusPhaseIncR = 0.73 / sampleRate;

    delayLine.prepare (spec);
    delayLine.reset();

    const auto fbLpf = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 3000.0f, 0.5f);
    *delayFbLpfL.coefficients = *fbLpf;
    *delayFbLpfR.coefficients = *fbLpf;
    delayFbLpfL.reset();
    delayFbLpfR.reset();

    // Wet shelf coefficients are recomputed on tone-param changes inside processBlock.
    lastDelayToneDb = -1000.0f;

    reverb.prepare (sampleRate, samplesPerBlock);
    reverb.reset();
    reverbBuffer.setSize (2, samplesPerBlock, false, false, true);
    lastReverbSize = -1.0f;
    lastReverbMod  = -1.0f;

    shimmer.prepare (sampleRate, samplesPerBlock);
    shimmer.reset();

    arpeggiator.prepare (sampleRate, samplesPerBlock);
    arpeggiator.reset();
    heldNotesScratch.reserve (16);

    drumMachine.prepare (sampleRate, samplesPerBlock);
    drumMachine.reset();

    transportSamples = 0;

    texture.prepare (sampleRate, samplesPerBlock);
    texture.reset();

    chordEvolver.prepare (sampleRate);

    widthPhase    = 0.0;
    widthPhaseInc = 0.3 / sampleRate;   // 0.3 Hz width LFO, matches standalone

    masterLpf.prepare (spec); masterLpf.reset();
    masterHpf.prepare (spec); masterHpf.reset();
    lastLpfHz = lastHpfHz = -1.0f;

    eqLow.prepare (spec);   eqLow.reset();
    eqLoMid.prepare (spec); eqLoMid.reset();
    eqHiMid.prepare (spec); eqHiMid.reset();
    eqHigh.prepare (spec);  eqHigh.reset();
    lastEqLowDb = lastEqLoMidDb = lastEqHiMidDb = lastEqHighDb = -1000.0f;

    masterGain.reset (sampleRate, 0.05); // 50 ms ramp
    masterGain.setCurrentAndTargetValue (masterVolParam->load());

    // Stop-fade ramp time: 1 second feels like the standalone's "Stop" pill.
    stopFade.reset (sampleRate, 1.0);
    stopFade.setCurrentAndTargetValue (1.0f);
}

void NorcoastAmbienceProcessor::releaseResources()
{
    chorusDelayL.reset(); chorusDelayR.reset();
    delayLine.reset();
    delayFbLpfL.reset(); delayFbLpfR.reset();
    delayWetShelfL.reset(); delayWetShelfR.reset();
    reverb.reset();
    shimmer.reset();
    eqLow.reset(); eqLoMid.reset(); eqHiMid.reset(); eqHigh.reset();
}

bool NorcoastAmbienceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& main = layouts.getMainOutputChannelSet();
    return main == juce::AudioChannelSet::stereo()
        || main == juce::AudioChannelSet::mono();
}

void NorcoastAmbienceProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const auto sr = getSampleRate();
    const int  n  = buffer.getNumSamples();

    // ─── Latch: drop all incoming note-off events before they reach
    // the keyboard state or the synths.
    if (latchOn.load (std::memory_order_acquire))
    {
        juce::MidiBuffer filtered;
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (! msg.isNoteOff() && ! (msg.isController() && msg.getControllerNumber() == 123))
                filtered.addEvent (msg, meta.samplePosition);
        }
        midi.swapWith (filtered);
    }

    // ─── Drone: ensure the homeRoot is always held when on, so the
    // synth auto-plays a chord on load like the standalone web app.
    {
        const bool droneOn = droneOnParam->load() > 0.5f;
        const int  rootIdx = juce::jlimit (0, 11, (int) homeRootParam->load());
        const int  desiredNote = 48 + rootIdx;          // C3 + offset

        if (droneOn)
        {
            if (currentDroneNote != desiredNote)
            {
                if (currentDroneNote >= 0)
                    midi.addEvent (juce::MidiMessage::noteOff (1, currentDroneNote), 0);
                midi.addEvent (juce::MidiMessage::noteOn (1, desiredNote, 0.7f), 0);
                currentDroneNote = desiredNote;
            }
        }
        else if (currentDroneNote >= 0)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, currentDroneNote), 0);
            currentDroneNote = -1;
        }
    }

    // Helper: takes whatever the previous step rendered into layerScratch,
    // applies the smoothed mute fade for that layer, and sums it onto the
    // main bus. Mute toggles ramp over 50 ms (configured in prepareToPlay)
    // so a click on the mute button doesn't actually click.
    auto sumLayerWithMute = [&] (juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& gain,
                                 std::atomic<float>* muteParam)
    {
        gain.setTargetValue (muteParam != nullptr && muteParam->load() > 0.5f ? 0.0f : 1.0f);
        auto* sL = layerScratch.getReadPointer (0);
        auto* sR = layerScratch.getReadPointer (1);
        auto* mL = buffer.getWritePointer (0);
        auto* mR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : mL;
        for (int s = 0; s < n; ++s)
        {
            const float g = gain.getNextValue();
            mL[s] += sL[s] * g;
            mR[s] += sR[s] * g;
        }
    };

    if (layerScratch.getNumSamples() < n)
        layerScratch.setSize (2, n, false, false, true);

    // ─── Foundation renders BEFORE the chord evolver runs, so it
    // receives the raw root note(s) only — no chord intervals.
    layerScratch.clear();
    foundationSynth.renderNextBlock (layerScratch, midi, 0, n);
    sumLayerWithMute (muteFoundation, foundationMuteParam);

    // ─── Chord Evolve: augment user note-ons with chord intervals,
    // optionally cycle the chord type on a timer. Runs BEFORE
    // keyboardState.processNextMidiBuffer so the augmented events get
    // recorded in the keyboard state — arp/texture/pads see them.
    // Tempo from host once — shared by evolve, arp, drums, delay.
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                bpm = *hostBpm;

    {
        const int targetType   = juce::jlimit (0, (int) ChordEvolver::NumTypes - 1,
                                               (int) chordTypeParam->load());
        const int customMask   = (int) customChordMaskParam->load();
        const int enabledMask  = (int) enabledChordsMaskParam->load();
        const bool evolveOn    = evolveOnParam->load() > 0.5f;

        // Evolve in bars (musical), 4/4 assumed → beats = bars * 4.
        static constexpr std::array<int, 6> kBars { 1, 2, 4, 8, 16, 32 };
        const int barsIdx = juce::jlimit (0, 5, (int) evolveBarsParam->load());
        const float beatsPerEvolve = (float) (kBars[(size_t) barsIdx] * 4);

        chordEvolver.process (midi, n, /*channel*/ 1,
                              targetType, customMask, enabledMask,
                              evolveOn, beatsPerEvolve, bpm,
                              transportSamples,
                              [this] (int newType)
                              {
                                  if (auto* p = apvts.getParameter (ParamID::chordType))
                                  {
                                      const auto range = p->getNormalisableRange();
                                      const float norm = range.convertTo0to1 ((float) newType);
                                      p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
                                  }
                              });
    }

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    layerScratch.clear();
    padsSynth.renderNextBlock (layerScratch, midi, 0, n);
    sumLayerWithMute (mutePads, padsMuteParam);

    layerScratch.clear();
    padsSynth2.renderNextBlock (layerScratch, midi, 0, n);
    sumLayerWithMute (mutePads2, pads2MuteParam);

    // ─── Arpeggiator (additive — same FX chain as the pads) ───────────
    {
        const float arpVol = arpVolParam->load();
        heldNotesScratch.clear();
        if (arpVol > 1e-4f)
            for (int note = 0; note < 128; ++note)
                if (keyboardState.isNoteOn (1, note))
                    heldNotesScratch.push_back (note);

        // Arp rate as choice index → beats per note.
        //   0 = 1/16  (0.25 beats)   1 = 1/8 (0.5)    2 = 1/4 (1.0)
        //   3 = 1/2   (2.0)          4 = 1 bar (4.0)
        static constexpr std::array<float, 5> kArpBeats { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
        const int rateIdx = juce::jlimit (0, 4, (int) arpRateParam->load());
        const float rate  = kArpBeats[(size_t) rateIdx];
        const int   octSpan = juce::jlimit (0, 2, (int) arpOctavesParam->load());
        const auto  voice   = static_cast<Arpeggiator::VoiceKind> (
                                  juce::jlimit (0, 2, (int) arpVoiceParam->load()));

        // Render to scratch + sum-with-mute so the mute button cuts the arp
        // cleanly; the shared transport keeps the step grid locked.
        layerScratch.clear();
        arpeggiator.process (layerScratch, 0, n, heldNotesScratch,
                             arpVol, rate, bpm, octSpan, voice,
                             transportSamples);
        sumLayerWithMute (muteArp, arpMuteParam);
    }

    // ─── Drums (additive — same FX chain as the pads) ─────────────────
    {
        const float drumVol = drumVolParam->load();
        const int   patIdx  = juce::jlimit (0, 5, (int) drumPatternParam->load());
        const int   loMask  = (int) drumCustomLoParam->load();
        const int   mdMask  = (int) drumCustomMdParam->load();
        const int   hhMask  = (int) drumCustomHhParam->load();
        const int   timeSig = juce::jlimit (0, 1, (int) timeSigParam->load());
        layerScratch.clear();
        drumMachine.process (layerScratch, 0, n, drumVol, patIdx, bpm,
                             loMask, mdMask, hhMask,
                             timeSig, transportSamples);
        sumLayerWithMute (muteDrum, drumMuteParam);
    }

    // ─── Texture (granular dulcimer — only fires while notes are held) ─
    {
        const float textureVol = textureVolParam->load();
        layerScratch.clear();
        if (textureVol > 1e-4f)
        {
            heldNotesScratch.clear();
            for (int note = 0; note < 128; ++note)
                if (keyboardState.isNoteOn (1, note))
                    heldNotesScratch.push_back (note);

            const bool octUp = textureOctUpParam->load() > 0.5f;
            texture.process (layerScratch, 0, n, heldNotesScratch, textureVol, octUp);
        }
        sumLayerWithMute (muteTexture, textureMuteParam);
    }

    // Snapshot params once per block.
    const float chorusMix   = chorusMixParam ->load();
    const float delayMix    = delayMixParam  ->load();
    const float delayFb     = delayFbParam   ->load();
    const int   delayDivIdx = juce::jlimit (0, 6, (int) delayDivParam->load());
    const float delayTone   = delayToneParam ->load();

    // BPM-locked delay time. Beat values for "1/32"…"1/4." (matches the
    // standalone's tempo-sync divisions).
    static constexpr std::array<float, 7> kDivBeats
        { 0.125f, 0.25f, 0.375f, 0.5f, 0.75f, 1.0f, 1.5f };
    const float delayTimeSec = (60.0f / (float) bpm) * kDivBeats[(size_t) delayDivIdx];
    const float reverbMix   = reverbMixParam ->load();
    const float reverbSize  = reverbSizeParam->load();
    const float reverbMod   = reverbModParam ->load();
    const float lpfFader    = lpfFreqParam   ->load();
    const float hpfFader    = hpfFreqParam   ->load();
    const float shimmerVol  = shimmerVolParam->load();
    const float widthMod    = widthModParam  ->load();
    const float satAmt      = satAmtParam    ->load();
    const float masterVol   = masterVolParam ->load();

    // ─── Update parameters whose coefficients/state are expensive ─────
    if (std::abs (reverbSize - lastReverbSize) > 1e-4f)
    {
        reverb.setDecay (reverbSizeToFB (reverbSize));
        lastReverbSize = reverbSize;
    }
    if (std::abs (reverbMod - lastReverbMod) > 1e-4f)
    {
        // Tamed from the standalone (was 0.3 + 1.7*r and 1.5*r) — at full
        // depth the modulation was wobbling pitches enough to hear, which
        // is musically too much for ambient pads.
        reverb.setExcursionRate    (0.2f + reverbMod * 0.6f);   // 0.2..0.8 Hz
        reverb.setExcursionDepthMs (reverbMod * 0.6f);          // 0..0.6 ms
        lastReverbMod = reverbMod;
    }

    const float toneDb = (delayTone - 0.5f) * 24.0f;   // ±12 dB shelf
    if (std::abs (toneDb - lastDelayToneDb) > 1e-3f)
    {
        const auto shelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sr, 1200.0f, 0.7071f, juce::Decibels::decibelsToGain (toneDb));
        *delayWetShelfL.coefficients = *shelf;
        *delayWetShelfR.coefficients = *shelf;
        lastDelayToneDb = toneDb;
    }

    delayLine.setDelay ((float) (sr * delayTimeSec));

    // Master HPF / LPF — log fader curves match the standalone helpers
    // lpfFaderToHz / hpfFaderToHz in public/index.html.
    const float lpfHz = std::exp (4.6051702f + lpfFader * lpfFader * 5.298375f);   // ln(100)..ln(20000)
    const float hpfHz = hpfFader < 0.01f
        ? 20.0f
        : std::exp (2.9957323f + hpfFader * 3.218876f);                            // ln(20)..ln(500)
    if (std::abs (lpfHz - lastLpfHz) > 0.5f)
    {
        *masterLpf.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
            sr, juce::jmin (lpfHz, (float) (sr * 0.49)), 0.7071f);
        lastLpfHz = lpfHz;
    }
    if (std::abs (hpfHz - lastHpfHz) > 0.1f)
    {
        *masterHpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
            sr, hpfHz, 0.7071f);
        lastHpfHz = hpfHz;
    }

    auto refreshEQ = [&] (StereoIIR& band, float& last, float db,
                          auto&& factory)
    {
        if (std::abs (db - last) > 1e-3f)
        {
            *band.state = *factory (db);
            last = db;
        }
    };
    refreshEQ (eqLow,   lastEqLowDb,   eqLowParam  ->load(),
               [sr](float db) {
                   return juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                       sr, 100.0f, 0.7071f, juce::Decibels::decibelsToGain (db));
               });
    refreshEQ (eqLoMid, lastEqLoMidDb, eqLoMidParam->load(),
               [sr](float db) {
                   return juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                       sr, 350.0f, 0.7f, juce::Decibels::decibelsToGain (db));
               });
    refreshEQ (eqHiMid, lastEqHiMidDb, eqHiMidParam->load(),
               [sr](float db) {
                   return juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                       sr, 2500.0f, 0.9f, juce::Decibels::decibelsToGain (db));
               });
    refreshEQ (eqHigh,  lastEqHighDb,  eqHighParam ->load(),
               [sr](float db) {
                   return juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                       sr, 8000.0f, 0.7071f, juce::Decibels::decibelsToGain (db));
               });

    // ─── Master HPF / LPF (pre-FX, matches standalone topology) ──────
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        masterHpf.process (ctx);
        masterLpf.process (ctx);
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;

    // ─── Juno chorus ──────────────────────────────────────────────────
    {
        const float chBaseL  = 0.007f  * (float) sr;
        const float chBaseR  = 0.009f  * (float) sr;
        const float chDepthL = 0.0025f * (float) sr;
        const float chDepthR = 0.003f  * (float) sr;
        const float chMix = chorusMix * 0.5f;

        constexpr float panAngL = (-0.65f + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        constexpr float panAngR = ( 0.65f + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        const float panL_L = std::cos (panAngL), panL_R = std::sin (panAngL);
        const float panR_L = std::cos (panAngR), panR_R = std::sin (panAngR);
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        for (int s = 0; s < n; ++s)
        {
            chorusPhaseL += chorusPhaseIncL; chorusPhaseL -= std::floor (chorusPhaseL);
            chorusPhaseR += chorusPhaseIncR; chorusPhaseR -= std::floor (chorusPhaseR);
            const float modL = std::sin ((float) chorusPhaseL * twoPi);
            const float modR = std::sin ((float) chorusPhaseR * twoPi);

            const float dryL = L[s];
            const float dryR = R[s];
            const float monoIn = (dryL + dryR) * 0.5f;

            chorusDelayL.pushSample (0, monoIn);
            chorusDelayR.pushSample (0, monoIn);
            chorusDelayL.setDelay (chBaseL + modL * chDepthL);
            chorusDelayR.setDelay (chBaseR + modR * chDepthR);
            const float chL = chorusDelayL.popSample (0);
            const float chR = chorusDelayR.popSample (0);

            L[s] = dryL + (chL * panL_L + chR * panR_L) * chMix;
            R[s] = dryR + (chL * panL_R + chR * panR_R) * chMix;
        }
    }

    // ─── Master delay (parallel send/return) ──────────────────────────
    {
        const float wetSend = delayMix * 0.7f;
        for (int s = 0; s < n; ++s)
        {
            float delayedL = delayLine.popSample (0);
            float delayedR = delayLine.popSample (1);

            delayedL = delayFbLpfL.processSample (delayedL);
            delayedR = delayFbLpfR.processSample (delayedR);

            const float dryL = L[s];
            const float dryR = R[s];

            delayLine.pushSample (0, dryL + delayedL * delayFb);
            delayLine.pushSample (1, dryR + delayedR * delayFb);

            const float wetL = delayWetShelfL.processSample (delayedL * wetSend);
            const float wetR = delayWetShelfR.processSample (delayedR * wetSend);

            L[s] = dryL + wetL;
            R[s] = dryR + wetR;
        }
    }

    // ─── Master reverb (Dattorro plate, parallel wet/dry) ─────────────
    {
        const int nch = buffer.getNumChannels();
        if (reverbBuffer.getNumSamples() < n)
            reverbBuffer.setSize (2, n, false, false, true);

        for (int ch = 0; ch < juce::jmin (nch, 2); ++ch)
            reverbBuffer.copyFrom (ch, 0, buffer, ch, 0, n);
        if (nch == 1)
            reverbBuffer.copyFrom (1, 0, buffer, 0, 0, n);

        reverb.processWet (reverbBuffer);

        for (int ch = 0; ch < nch; ++ch)
        {
            auto* dst = buffer.getWritePointer (ch);
            auto* wet = reverbBuffer.getReadPointer (juce::jmin (ch, 1));
            for (int i = 0; i < n; ++i)
                dst[i] = dst[i] * (1.0f - reverbMix) + wet[i] * reverbMix;
        }
    }

    // ─── Shimmer (parallel add — pitched feedback Dattorro) ───────────
    // Placed after the main reverb so the shimmer's tail isn't dunked
    // back through the reverb's wet/dry mixer (which would attenuate
    // it by 1-reverbMix). Slightly different topology to the
    // standalone (which sums shimmer + main-reverb in parallel) but
    // sonically equivalent for this preset's mix balance.
    shimmer.processAdd (buffer, shimmerVol * 1.5f);

    // ─── Width LFO (master pan modulation, 0.3 Hz) ────────────────────
    if (widthMod > 1e-4f)
    {
        const float depth = widthMod * 0.5f;   // ×0.5 internal scale (matches standalone)
        constexpr float halfPi = juce::MathConstants<float>::halfPi;
        for (int s = 0; s < n; ++s)
        {
            widthPhase += widthPhaseInc;
            if (widthPhase >= 1.0) widthPhase -= 1.0;
            const float panAmt = depth * std::sin ((float) widthPhase
                                                    * juce::MathConstants<float>::twoPi);
            const float dryL = L[s];
            const float dryR = R[s];
            // Web Audio StereoPanner with stereo input — exact equation.
            if (panAmt < 0.0f)
            {
                const float p = -panAmt * halfPi;
                L[s] = dryL + dryR * std::sin (p);
                R[s] = dryR * std::cos (p);
            }
            else
            {
                const float p = panAmt * halfPi;
                L[s] = dryL * std::cos (p);
                R[s] = dryL * std::sin (p) + dryR;
            }
        }
    }

    // ─── EQ ───────────────────────────────────────────────────────────
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        eqLow.process   (ctx);
        eqLoMid.process (ctx);
        eqHiMid.process (ctx);
        eqHigh.process  (ctx);
    }

    // ─── Saturation (tanh soft-clip) ──────────────────────────────────
    if (satAmt > 0.005f)
    {
        const float k    = satAmt * 3.5f;
        const float invN = 1.0f / std::tanh (k);
        const float makeup = 1.0f - satAmt * 0.3f;
        const float scale = invN * makeup;
        for (int s = 0; s < n; ++s)
        {
            L[s] = std::tanh (L[s] * k) * scale;
            R[s] = std::tanh (R[s] * k) * scale;
        }
    }

    // ─── Master volume + Stop fade ────────────────────────────────────
    masterGain.setTargetValue (masterVol);
    stopFade .setTargetValue (stopped.load (std::memory_order_acquire) ? 0.0f : 1.0f);
    for (int s = 0; s < n; ++s)
    {
        const float g = masterGain.getNextValue() * stopFade.getNextValue();
        L[s] *= g;
        R[s] *= g;
    }

    // Advance the shared transport clock for the next block. All grid-locked
    // modules (arp, drums, evolve) read this at block-start and stay phase
    // aligned with each other regardless of when notes are pressed.
    transportSamples += n;
}

juce::AudioProcessorEditor* NorcoastAmbienceProcessor::createEditor()
{
    return new NorcoastAmbienceEditor (*this);
}

// ─── Host-visible factory programs ────────────────────────────────────
int NorcoastAmbienceProcessor::getNumPrograms()
{
    return (int) Presets::factory().size();
}

const juce::String NorcoastAmbienceProcessor::getProgramName (int idx)
{
    const auto& list = Presets::factory();
    if (idx < 0 || idx >= (int) list.size()) return {};
    return list[(size_t) idx].name;
}

void NorcoastAmbienceProcessor::setCurrentProgram (int idx)
{
    const auto& list = Presets::factory();
    if (idx < 0 || idx >= (int) list.size()) return;
    Presets::apply (apvts, list[(size_t) idx]);
    currentProgram = idx;

    // Notify any AudioProcessorListeners — host preset menus rely on this
    // to refresh their displayed program-name when state changes.
    updateHostDisplay();
}

void NorcoastAmbienceProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, dest);
    }
}

void NorcoastAmbienceProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NorcoastAmbienceProcessor();
}
