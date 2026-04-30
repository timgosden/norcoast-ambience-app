#include "PluginProcessor.h"
#include "PluginEditor.h"

NorcoastAmbienceProcessor::NorcoastAmbienceProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void NorcoastAmbienceProcessor::prepareToPlay (double, int) {}
void NorcoastAmbienceProcessor::releaseResources() {}

bool NorcoastAmbienceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& main = layouts.getMainOutputChannelSet();
    return main == juce::AudioChannelSet::stereo()
        || main == juce::AudioChannelSet::mono();
}

void NorcoastAmbienceProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // Phase 1 skeleton — silent output. DSP arrives in phase 2 (Foundation pad layer).
    buffer.clear();
}

juce::AudioProcessorEditor* NorcoastAmbienceProcessor::createEditor()
{
    return new NorcoastAmbienceEditor (*this);
}

void NorcoastAmbienceProcessor::getStateInformation (juce::MemoryBlock&) {}
void NorcoastAmbienceProcessor::setStateInformation (const void*, int) {}

// JUCE plugin entry point — invoked by every wrapper format.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NorcoastAmbienceProcessor();
}
