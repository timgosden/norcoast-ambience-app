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
    reverb.prepare (spec);
    reverb.reset();
}

void NorcoastAmbienceProcessor::releaseResources()
{
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

    // Master reverb — wet/dry handled internally by juce::dsp::Reverb.
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
