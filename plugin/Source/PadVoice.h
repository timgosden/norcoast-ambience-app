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
    // `gainParam` and `extraSubOctaveParam` must outlive the voice — they
    // point into APVTS atomic-float storage owned by the AudioProcessor.
    // When `extraSubOctaveParam > 0.5` at note-on time, the voice plays an
    // extra octave below the lowest configured one.
    PadVoice (const LayerConfig& cfg,
              std::atomic<float>* gainParam,
              std::atomic<float>* extraSubOctaveParam   = nullptr,
              std::atomic<float>* velocitySensParam     = nullptr,
              std::atomic<float>* pitchBendRangeParam   = nullptr,
              std::atomic<float>* extraSuperOctaveParam = nullptr);
    ~PadVoice() override = default;

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<PadSound*> (s) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int /*pitchWheelPosition*/) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newValue) override
    {
        // newValue is 0..16383 (centred at 8192); convert to ±1.
        pitchBendNormalised = ((float) newValue - 8192.0f) / 8192.0f;
    }
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override;

    // Override the ADSR release (in seconds) — called from the
    // processor every block so the user-controlled "Key Xfade" slider
    // determines how long held notes take to fade after a noteOff.
    void setReleaseSeconds (float secs)
    {
        auto p = adsr.getParameters();
        p.release = juce::jlimit (0.05f, 8.0f, secs);
        adsr.setParameters (p);
    }

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
        // 0 = regular octave, 1 = extra sub-oct, 2 = extra super-oct.
        // Lets renderNextBlock fade the extra octaves in/out from the
        // toggle params smoothly without retriggering the voice.
        int              octGroup     = 0;
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
    std::atomic<float>* layerGainParam        = nullptr;
    std::atomic<float>* extraSubOctaveParam   = nullptr;
    std::atomic<float>* velocitySensParam     = nullptr;
    std::atomic<float>* pitchBendRangeParam   = nullptr;
    std::atomic<float>* extraSuperOctaveParam = nullptr;
    float               velocityScale        = 1.0f;     // captured at note-on
    float               pitchBendNormalised  = 0.0f;     // -1..+1 from wheel
    std::vector<Osc>    oscs;
    int                 activeOscCount       = 0;
    OnePoleLP filterL, filterR;
    LFO filterLFO1, filterLFO2, ampLFO, breathLFO;
    float filterBaseHz = 0.0f;
    juce::ADSR adsr;

    // Smoothed gains for the optional extra-octave groups so the toggle
    // buttons fade those oscs in/out without clicks or retriggering.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> subOctGain   { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> superOctGain { 0.0f };

    // Smoothed layer-volume read so a preset switch ramps the per-voice
    // gain in over ~30 ms instead of jumping (which used to click).
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> layerGainSmoothed { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadVoice)
};
