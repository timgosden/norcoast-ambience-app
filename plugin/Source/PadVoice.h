#pragma once

#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PadSound.h"
#include "LayerConfig.h"
#include "Wavetable.h"

// Generic pad voice — driven entirely by a LayerConfig so a single class
// covers Foundation, Pads, and any future synth layers. Same DSP topology
// as phase 2c:
//   - N oscillators (timbres × counts × octaves) with detune spread + stereo pan.
//   - 1-pole lowpass per channel at fBase * 1.5, modulated by two filter LFOs.
//   - Amp LFO + breath LFO modulating output level.
//   - Per-voice detune LFOs for slow chorus-y movement between oscillators.
//   - Slow ADSR (per-layer parameters).
//
// Block-rate modulation (LFOs ticked once per processBlock).
class PadVoice : public juce::SynthesiserVoice
{
public:
    // `gainParam` must outlive the voice — it points into the APVTS atomic
    // float store owned by the AudioProcessor.
    PadVoice (const LayerConfig& cfg, std::atomic<float>* gainParam);
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
        double           baseHz       = 0.0;
        float            staticCents  = 0.0f;
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
            hz = juce::jlimit (10.0f, (float) (sr * 0.45), hz);
            a  = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr);
        }
        float process (float in) noexcept { z += a * (in - z); return z; }
        void  reset()             noexcept { z = 0.0f; }
    };

private:
    const LayerConfig& cfg;
    std::atomic<float>* layerGainParam = nullptr;
    std::vector<Osc>    oscs;
    OnePoleLP filterL, filterR;
    LFO filterLFO1, filterLFO2, ampLFO, breathLFO;
    float filterBaseHz = 0.0f;
    juce::ADSR adsr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadVoice)
};
