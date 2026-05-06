#pragma once

#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "DattorroReverb.h"

// Shimmer v2 — clean, in-tune, predictable.
//
// The previous granular-pitch-shift-with-feedback approach kept drifting
// out of tune as artefacts compounded across feedback passes. Replaced
// with the standard "ambient bright tail" recipe:
//
//   input → HPF (1500 Hz) → bright Dattorro reverb (long decay,
//          low damping) → wide stereo output
//
// No pitch-shift loop, no feedback compounding — just a parallel
// reverb send fed only the upper harmonics. Sounds shimmery without
// going off-pitch. Quiet by default; the wetMix arg scales the send.
class Shimmer
{
public:
    void prepare (double sr, int blockSize)
    {
        sampleRate = sr;

        inner.prepare (sr, blockSize);
        inner.setBandwidth        (0.9999f);
        inner.setDamping          (0.04f);   // very bright
        inner.setDecay            (0.85f);   // long tail
        inner.setExcursionRate    (0.35f);
        inner.setExcursionDepthMs (0.6f);
        inner.setPreDelaySamples  ((int) (0.005 * sr));
        inner.reset();

        const auto hpf = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 1500.0f, 0.5f);
        *highpass.coefficients = *hpf;
        highpass.reset();
    }

    void reset()
    {
        inner.reset();
        highpass.reset();
    }

    // Adds the shimmer wet output to `mainBuffer`. wetMix is 0..1 (and
    // we apply an internal 0.7 scale so even at full it stays musical).
    void processAdd (juce::AudioBuffer<float>& mainBuffer, float wetMix)
    {
        if (wetMix < 1e-4f) return;

        const int n   = mainBuffer.getNumSamples();
        const int nch = mainBuffer.getNumChannels();
        auto* L = mainBuffer.getWritePointer (0);
        auto* R = nch > 1 ? mainBuffer.getWritePointer (1) : L;

        const float send = wetMix * 0.7f;

        for (int i = 0; i < n; ++i)
        {
            // Bright send — only the high band feeds the shimmer reverb.
            const float monoIn = (L[i] + R[i]) * 0.5f;
            const float bright = highpass.processSample (monoIn);

            float wetL, wetR;
            inner.processSampleStereo (bright, wetL, wetR);

            L[i] += wetL * send;
            R[i] += wetR * send;
        }
    }

private:
    double sampleRate = 44100.0;
    DattorroReverb inner;
    juce::dsp::IIR::Filter<float> highpass;
};
