#include "PadVoice.h"

void PadVoice::startNote (int midiNoteNumber, float velocity,
                          juce::SynthesiserSound*, int)
{
    const auto sr = getSampleRate();
    const auto freq = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

    phase          = 0.0;
    phaseIncrement = juce::MathConstants<double>::twoPi * freq / sr;
    level          = juce::jlimit (0.05f, 1.0f, velocity);

    adsr.setSampleRate (sr);
    adsr.setParameters (adsrParams);
    adsr.noteOn();
}

void PadVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        adsr.reset();
        clearCurrentNote();
    }
}

void PadVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                int startSample, int numSamples)
{
    if (! adsr.isActive())
        return;

    constexpr float headroom = 0.18f; // -15 dB so polyphony doesn't slam the limiter

    for (int s = 0; s < numSamples; ++s)
    {
        const float env    = adsr.getNextSample();
        const float sample = (float) std::sin (phase) * level * headroom * env;

        phase += phaseIncrement;
        if (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;

        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            outputBuffer.addSample (ch, startSample + s, sample);
    }

    if (! adsr.isActive())
        clearCurrentNote();
}
