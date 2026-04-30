#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PadSound.h"
#include "PadVoice.h"

namespace
{
    // Direct port of the first two entries in PAD_LAYERS from the standalone
    // (public/index.html). layerGain matches DEFAULT_VOLUMES.
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
}

NorcoastAmbienceProcessor::NorcoastAmbienceProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      foundationConfig (makeFoundationConfig()),
      padsConfig       (makePadsConfig())
{
    Waves::init();

    foundationSynth.addSound (new PadSound());
    padsSynth      .addSound (new PadSound());

    for (int i = 0; i < kVoicesPerLayer; ++i)
    {
        foundationSynth.addVoice (new PadVoice (foundationConfig));
        padsSynth      .addVoice (new PadVoice (padsConfig));
    }

    // Master delay — defaults match the standalone:
    //   delayTimeVal = 60/70 s (quarter @ 70 BPM, DELAY_Q in the standalone)
    //   delayMixVal  = 0.46  (wet send, internally scaled ×0.7 like the standalone)
    //   delayFeedback = 0.57
    //   feedback path is low-passed at 3 kHz, Q 0.5
    //   wet send is run through a +12 dB high-shelf at 1200 Hz
    //     (standalone's `dToneNode` at delayTone = 1.0)

    // Master plate reverb — defaults match the standalone:
    //   reverbMix=0.71, reverbSize=0.92.
    // juce::dsp::Reverb is Schroeder-style (not Dattorro), so this is a
    // close-enough approximation for now; a Dattorro port can replace it
    // later without changing the surrounding plumbing.
    juce::dsp::Reverb::Parameters p;
    p.roomSize   = 0.92f;
    p.damping    = 0.3f;     // plate-ish — relatively bright tail
    p.wetLevel   = 0.71f;
    p.dryLevel   = 1.0f - 0.71f;
    p.width      = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);
}

void NorcoastAmbienceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    foundationSynth.setCurrentPlaybackSampleRate (sampleRate);
    padsSynth      .setCurrentPlaybackSampleRate (sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    // Chorus delays are mono each — fed a mono sum of L+R.
    juce::dsp::ProcessSpec monoSpec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    chorusDelayL.prepare (monoSpec);
    chorusDelayR.prepare (monoSpec);
    chorusDelayL.reset();
    chorusDelayR.reset();
    chorusPhaseIncL = 0.55 / sampleRate;
    chorusPhaseIncR = 0.73 / sampleRate;

    // Delay
    delayLine.prepare (spec);
    delayLine.setDelay ((float) (sampleRate * (60.0 / 70.0))); // DELAY_Q
    delayLine.reset();

    const auto fbLpf = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 3000.0f, 0.5f);
    *delayFbLpfL.coefficients = *fbLpf;
    *delayFbLpfR.coefficients = *fbLpf;
    delayFbLpfL.reset();
    delayFbLpfR.reset();

    const auto shelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 1200.0f, 0.7071f, juce::Decibels::decibelsToGain (12.0f));
    *delayWetShelfL.coefficients = *shelf;
    *delayWetShelfR.coefficients = *shelf;
    delayWetShelfL.reset();
    delayWetShelfR.reset();

    // Reverb
    reverb.prepare (spec);
    reverb.reset();
}

void NorcoastAmbienceProcessor::releaseResources()
{
    chorusDelayL.reset(); chorusDelayR.reset();
    delayLine.reset();
    delayFbLpfL.reset(); delayFbLpfR.reset();
    delayWetShelfL.reset(); delayWetShelfR.reset();
    reverb.reset();
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

    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    foundationSynth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
    padsSynth      .renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    const auto sr = getSampleRate();

    // ─── Juno-style stereo chorus ─────────────────────────────────────
    // Two modulated delays:
    //   dL: 7 ms ± 2.5 ms @ 0.55 Hz, panned -0.65
    //   dR: 9 ms ± 3   ms @ 0.73 Hz, panned +0.65
    // Wet send = chorusMix * 0.5 = 0.175 (chorusMix = 0.35 default).
    {
        const float chBaseL  = 0.007f  * (float) sr;
        const float chBaseR  = 0.009f  * (float) sr;
        const float chDepthL = 0.0025f * (float) sr;
        const float chDepthR = 0.003f  * (float) sr;
        constexpr float chMix = 0.35f * 0.5f;

        constexpr float panAngL = (-0.65f + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        constexpr float panAngR = ( 0.65f + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        const float panL_L = std::cos (panAngL), panL_R = std::sin (panAngL);
        const float panR_L = std::cos (panAngR), panR_R = std::sin (panAngR);
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        const int n = buffer.getNumSamples();
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;

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

    // ─── Master delay ─────────────────────────────────────────────────
    // Matches the standalone topology exactly:
    //   delayed → LPF (3 kHz, Q 0.5) → feedback (×0.57) → back into delay
    //                                → wet send (×mix*0.7) → high-shelf → mix
    constexpr float kFeedback = 0.57f;
    constexpr float kWetSend  = 0.46f * 0.7f;   // delayMixVal * standalone's 0.7 scale

    const int n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;

    for (int s = 0; s < n; ++s)
    {
        float delayedL = delayLine.popSample (0);
        float delayedR = delayLine.popSample (1);

        delayedL = delayFbLpfL.processSample (delayedL);
        delayedR = delayFbLpfR.processSample (delayedR);

        const float dryL = L[s];
        const float dryR = R[s];

        delayLine.pushSample (0, dryL + delayedL * kFeedback);
        delayLine.pushSample (1, dryR + delayedR * kFeedback);

        const float wetL = delayWetShelfL.processSample (delayedL * kWetSend);
        const float wetR = delayWetShelfR.processSample (delayedR * kWetSend);

        L[s] = dryL + wetL;
        R[s] = dryR + wetR;
    }

    // ─── Master reverb ────────────────────────────────────────────────
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    reverb.process (ctx);
}

juce::AudioProcessorEditor* NorcoastAmbienceProcessor::createEditor()
{
    return new NorcoastAmbienceEditor (*this);
}

void NorcoastAmbienceProcessor::getStateInformation (juce::MemoryBlock&) {}
void NorcoastAmbienceProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NorcoastAmbienceProcessor();
}
