#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PadSound.h"
#include "PadVoice.h"
#include "Parameters.h"

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
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    Waves::init();

    // Cache atom pointers for realtime-safe param reads.
    foundationVolParam = apvts.getRawParameterValue (ParamID::foundationVol);
    padsVolParam       = apvts.getRawParameterValue (ParamID::padsVol);
    chorusMixParam     = apvts.getRawParameterValue (ParamID::chorusMix);
    delayMixParam      = apvts.getRawParameterValue (ParamID::delayMix);
    delayFbParam       = apvts.getRawParameterValue (ParamID::delayFb);
    delayTimeMsParam   = apvts.getRawParameterValue (ParamID::delayTimeMs);
    delayToneParam     = apvts.getRawParameterValue (ParamID::delayTone);
    reverbMixParam     = apvts.getRawParameterValue (ParamID::reverbMix);
    reverbSizeParam    = apvts.getRawParameterValue (ParamID::reverbSize);
    reverbModParam     = apvts.getRawParameterValue (ParamID::reverbMod);
    shimmerVolParam    = apvts.getRawParameterValue (ParamID::shimmerVol);
    widthModParam      = apvts.getRawParameterValue (ParamID::widthMod);
    satAmtParam        = apvts.getRawParameterValue (ParamID::satAmt);
    masterVolParam     = apvts.getRawParameterValue (ParamID::masterVol);
    eqLowParam         = apvts.getRawParameterValue (ParamID::eqLow);
    eqLoMidParam       = apvts.getRawParameterValue (ParamID::eqLoMid);
    eqHiMidParam       = apvts.getRawParameterValue (ParamID::eqHiMid);
    eqHighParam        = apvts.getRawParameterValue (ParamID::eqHigh);

    foundationSynth.addSound (new PadSound());
    padsSynth      .addSound (new PadSound());

    for (int i = 0; i < kVoicesPerLayer; ++i)
    {
        foundationSynth.addVoice (new PadVoice (foundationConfig, foundationVolParam));
        padsSynth      .addVoice (new PadVoice (padsConfig,       padsVolParam));
    }
}

void NorcoastAmbienceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    foundationSynth.setCurrentPlaybackSampleRate (sampleRate);
    padsSynth      .setCurrentPlaybackSampleRate (sampleRate);

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

    widthPhase    = 0.0;
    widthPhaseInc = 0.3 / sampleRate;   // 0.3 Hz width LFO, matches standalone

    eqLow.prepare (spec);   eqLow.reset();
    eqLoMid.prepare (spec); eqLoMid.reset();
    eqHiMid.prepare (spec); eqHiMid.reset();
    eqHigh.prepare (spec);  eqHigh.reset();
    lastEqLowDb = lastEqLoMidDb = lastEqHiMidDb = lastEqHighDb = -1000.0f;

    masterGain.reset (sampleRate, 0.05); // 50 ms ramp
    masterGain.setCurrentAndTargetValue (masterVolParam->load());
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

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    foundationSynth.renderNextBlock (buffer, midi, 0, n);
    padsSynth      .renderNextBlock (buffer, midi, 0, n);

    // Snapshot params once per block.
    const float chorusMix   = chorusMixParam ->load();
    const float delayMix    = delayMixParam  ->load();
    const float delayFb     = delayFbParam   ->load();
    const float delayTimeMs = delayTimeMsParam->load();
    const float delayTone   = delayToneParam ->load();
    const float reverbMix   = reverbMixParam ->load();
    const float reverbSize  = reverbSizeParam->load();
    const float reverbMod   = reverbModParam ->load();
    const float shimmerVol  = shimmerVolParam->load();
    const float widthMod    = widthModParam  ->load();
    const float satAmt      = satAmtParam    ->load();
    const float masterVol   = masterVolParam ->load();

    // ─── Update parameters whose coefficients/state are expensive ─────
    if (reverbSize != lastReverbSize)
    {
        reverb.setDecay (reverbSizeToFB (reverbSize));
        lastReverbSize = reverbSize;
    }
    if (reverbMod != lastReverbMod)
    {
        reverb.setExcursionRate    (0.3f + reverbMod * 1.7f);
        reverb.setExcursionDepthMs (reverbMod * 1.5f);
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

    delayLine.setDelay ((float) (sr * delayTimeMs * 0.001));

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

    // ─── Master volume (smoothed) ─────────────────────────────────────
    masterGain.setTargetValue (masterVol);
    for (int s = 0; s < n; ++s)
    {
        const float g = masterGain.getNextValue();
        L[s] *= g;
        R[s] *= g;
    }
}

juce::AudioProcessorEditor* NorcoastAmbienceProcessor::createEditor()
{
    return new NorcoastAmbienceEditor (*this);
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
