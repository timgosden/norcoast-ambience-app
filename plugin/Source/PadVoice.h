#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PadSound.h"
#include "Wavetable.h"

// Phase 2b voice: Foundation pad layer.
//   - 5 oscillators (3 "sub" voices + 2 "warm" voices)
//   - Per-osc detune spread and stereo pan (matches standalone formula)
//   - Single 1-pole lowpass per channel at fBase * 1.5 = 300 Hz
//   - Slow ADSR (7 s attack / 7 s release)
//   - Plays one octave below MIDI note (Foundation's octaves[-1])
//
// LFO modulation lands in phase 2c. Pads / Texture / FX in phase 3.
class PadVoice : public juce::SynthesiserVoice
{
public:
    PadVoice() = default;
    ~PadVoice() override = default;

    // Build the shared sub/warm wavetables. Call once before any voice plays.
    static void initWavetables();

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
    static Wavetable subTable, warmTable;

    struct Osc
    {
        const Wavetable* table    = nullptr;
        double           phase    = 0.0;
        double           phaseInc = 0.0;
        float            gain     = 0.0f;
        float            panL     = 1.0f / std::sqrt (2.0f); // pre-computed pan gains
        float            panR     = 1.0f / std::sqrt (2.0f);
    };

    struct OnePoleLP
    {
        float a = 0.0f, z = 0.0f;
        void  setCutoff (double sr, float hz)
        {
            a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr);
        }
        float process (float in) noexcept { z += a * (in - z); return z; }
        void  reset() noexcept { z = 0.0f; }
    };

    std::array<Osc, 5> oscs;
    OnePoleLP filterL, filterR;

    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams { 7.0f, 0.5f, 1.0f, 7.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadVoice)
};
