#pragma once

#include <array>
#include <cmath>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "DattorroReverb.h"

// Shimmer v3 — proper octave-up sparkle.
//
// Signal path:
//   input → HPF (700 Hz) → +12 st granular pitch shifter
//         → bright Dattorro reverb (long decay, very low damping)
//         → wet out (mixed against dry by `wetMix`)
//
// The pitch shifter sits BEFORE the reverb (not in a feedback loop),
// so artefacts don't compound — the reverb smears them into a
// perpetual sparkly tail without drifting out of tune. Two Hann-
// windowed grains at 50% overlap give clean +12 st at low CPU.
class Shimmer
{
public:
    void prepare (double sr, int blockSize)
    {
        sampleRate = sr;

        inner.prepare (sr, blockSize);
        inner.setBandwidth        (0.9999f);
        inner.setDamping          (0.03f);   // very bright tail
        inner.setDecay            (0.88f);   // long, washy
        inner.setExcursionRate    (0.35f);
        inner.setExcursionDepthMs (0.6f);
        inner.setPreDelaySamples  ((int) (0.005 * sr));
        inner.reset();

        const auto hpf = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 700.0f, 0.5f);
        *highpass.coefficients = *hpf;
        highpass.reset();

        pitchBuf.assign (kBufSize, 0.0f);
        writePos = 0;
        readPos[0] = 0.0;
        readPos[1] = (double) (kGrainSize / 2);
    }

    void reset()
    {
        inner.reset();
        highpass.reset();
        std::fill (pitchBuf.begin(), pitchBuf.end(), 0.0f);
        writePos = 0;
        readPos[0] = 0.0;
        readPos[1] = (double) (kGrainSize / 2);
    }

    // Adds the shimmer wet output to mainBuffer. wetMix scales the
    // octave-up reverb send into the main bus.
    void processAdd (juce::AudioBuffer<float>& mainBuffer, float wetMix)
    {
        if (wetMix < 1e-4f) return;

        const int n   = mainBuffer.getNumSamples();
        const int nch = mainBuffer.getNumChannels();
        auto* L = mainBuffer.getWritePointer (0);
        auto* R = nch > 1 ? mainBuffer.getWritePointer (1) : L;

        const float send = wetMix * 0.85f;

        for (int i = 0; i < n; ++i)
        {
            // Mono pre-shift signal — high-passed so the shimmer only
            // works on harmonics above the fundamental.
            const float monoIn = (L[i] + R[i]) * 0.5f;
            const float bright = highpass.processSample (monoIn);

            // +12 st pitch shift via two Hann-windowed grains at 50%
            // overlap, read at 2× write rate.
            const float pitched = pitchShiftStep (bright);

            // Pitched signal feeds the bright reverb.
            float wetL, wetR;
            inner.processSampleStereo (pitched, wetL, wetR);

            L[i] += wetL * send;
            R[i] += wetR * send;
        }
    }

private:
    static constexpr int kBufSize    = 4096;     // ~93 ms @ 44.1k
    static constexpr int kGrainSize  = 1024;     // ~23 ms grain

    inline float pitchShiftStep (float in) noexcept
    {
        pitchBuf[(size_t) writePos] = in;

        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        const float invG  = 1.0f / (float) kGrainSize;

        float out = 0.0f;
        for (int g = 0; g < 2; ++g)
        {
            const float window = 0.5f * (1.0f - std::cos (twoPi * (float) readPos[g] * invG));

            const double exact = (double) writePos - (double) kGrainSize + readPos[g];
            const double wrapped = exact - std::floor (exact / (double) kBufSize)
                                          * (double) kBufSize;
            const int   idx0 = (int) wrapped;
            const int   idx1 = (idx0 + 1) % kBufSize;
            const float frac = (float) (wrapped - (double) idx0);
            const float s    = pitchBuf[(size_t) idx0] * (1.0f - frac)
                             + pitchBuf[(size_t) idx1] * frac;

            out += s * window;

            readPos[g] += 2.0;                    // 2× rate ⇒ +12 st
            if (readPos[g] >= (double) kGrainSize)
                readPos[g] -= (double) kGrainSize;
        }

        writePos = (writePos + 1) % kBufSize;
        return out;                                // sum of two Hann grains ≈ unity
    }

    double sampleRate = 44100.0;
    DattorroReverb inner;
    juce::dsp::IIR::Filter<float> highpass;

    std::vector<float> pitchBuf;
    std::array<double, 2> readPos { 0.0, (double) (kGrainSize / 2) };
    int writePos = 0;
};
