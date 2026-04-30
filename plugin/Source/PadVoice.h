#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PadSound.h"

// Phase 2a voice: a single sine oscillator with a slow ADSR. The ATTACK
// matches the standalone's 7-second pad fade-in so the character is right
// while we still have nothing else going on. Replaced in phase 2b by a
// proper 5-voice timbre stack with detune + filter + LFO modulation.
class PadVoice : public juce::SynthesiserVoice
{
public:
    PadVoice() = default;
    ~PadVoice() override = default;

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<PadSound*> (s) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int /*pitchWheelPosition*/) override;

    void stopNote (float velocity, bool allowTailOff) override;

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override;

private:
    double phase           = 0.0;
    double phaseIncrement  = 0.0;
    float  level           = 0.0f;

    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams { 7.0f, 0.5f, 1.0f, 7.0f }; // attack / decay / sustain / release (s)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadVoice)
};
