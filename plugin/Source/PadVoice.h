#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PadSound.h"
#include "Wavetable.h"

// Phase 2c voice: Foundation pad layer with full LFO modulation.
//   - 5 oscillators (3 sub + 2 warm) with detune spread + stereo pan.
//   - 1-pole lowpass at fBase * 1.5 = 300 Hz, modulated by two filter LFOs.
//   - Amp LFO + breath LFO modulating output level.
//   - Per-voice detune LFOs for slow chorus-y movement between oscillators.
//   - Slow ADSR (7 s / 7 s).
//
// Block-rate modulation (LFOs ticked once per processBlock) — at 0.01-0.5 Hz
// the per-block step is sub-percent so the audio is smooth.
class PadVoice : public juce::SynthesiserVoice
{
public:
    PadVoice() = default;
    ~PadVoice() override = default;

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

    // Slow LFO with arbitrary depth and start phase.
    struct LFO
    {
        double phase    = 0.0;
        double phaseInc = 0.0;
        float  depth    = 0.0f;

        void setup (double sampleRate, double rateHz, float depthVal, double startPhase = 0.0)
        {
            phase    = startPhase;
            phaseInc = rateHz / sampleRate;
            depth    = depthVal;
        }

        // Advance `samples` audio frames and return the LFO output (sine * depth).
        float advance (int samples) noexcept
        {
            phase += phaseInc * (double) samples;
            phase -= std::floor (phase);
            return (float) std::sin (phase * juce::MathConstants<double>::twoPi) * depth;
        }
    };

    struct Osc
    {
        const Wavetable* table        = nullptr;
        double           phase        = 0.0;
        double           phaseInc     = 0.0;
        double           baseHz       = 0.0;   // freq before detune
        float            staticCents  = 0.0f;  // constant per-voice detune offset
        float            gain         = 0.0f;
        float            panL         = 1.0f / std::sqrt (2.0f);
        float            panR         = 1.0f / std::sqrt (2.0f);
        LFO              detuneLFO;
    };

    struct OnePoleLP
    {
        float a = 0.0f, z = 0.0f;
        void  setCutoff (double sr, float hz)
        {
            // Clamp cutoff to a sane band — LFO mod can briefly drag it below
            // 1 Hz in extreme cases which would NaN the coefficient.
            hz = juce::jlimit (10.0f, (float) (sr * 0.45), hz);
            a  = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr);
        }
        float process (float in) noexcept { z += a * (in - z); return z; }
        void  reset()             noexcept { z = 0.0f; }
    };

    std::array<Osc, 5> oscs;
    OnePoleLP filterL, filterR;

    // Master modulators
    LFO filterLFO1, filterLFO2, ampLFO, breathLFO;
    float filterBaseHz = 300.0f;

    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams { 7.0f, 0.5f, 1.0f, 7.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadVoice)
};
