#pragma once

#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "DattorroReverb.h"

// Shimmer reverb — port of the standalone's createShimmerReverb()
// (public/index.html). A second Dattorro instance with a feedback
// loop that pitch-shifts up an octave on every pass:
//
//   input → reverb in
//   reverb out → HPF 180 Hz → LPF 6.5 kHz → +12 st pitch shift
//              → small chorus → reverb in (sums)
//   reverb out → Haas spread (11 ms / 19 ms, panned ±0.8) → wet send
//
// The pitch shifter is a simple 50%-overlap granular implementation
// (two Hann-windowed grains, half-buffer offset, played back at 2×
// the input rate). Phase-vocoder-grade clarity isn't necessary — the
// inner reverb's diffusion masks granular artefacts.
class Shimmer
{
public:
    void prepare (double sr, int blockSize)
    {
        sampleRate = sr;

        // Inner Dattorro — bright, medium decay (params from
        // createShimmerReverb in the standalone).
        inner.prepare (sr, blockSize);
        inner.setBandwidth        (0.9999f);
        inner.setDamping          (0.06f);
        inner.setDecay            (0.72f);
        inner.setExcursionRate    (0.4f);
        inner.setExcursionDepthMs (0.5f);
        inner.setPreDelaySamples  ((int) (0.005 * sr));
        inner.reset();

        // Feedback HPF / LPF
        const auto hpf = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 180.0f, 0.5f);
        const auto lpf = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, 6500.0f, 0.5f);
        *fbHpf.coefficients = *hpf;
        *fbLpf.coefficients = *lpf;
        fbHpf.reset();
        fbLpf.reset();

        // Granular pitch shifter buffer
        pitchBuf.assign (kPitchBufSize, 0.0f);
        for (int g = 0; g < kNumGrains; ++g)
            readPos[(size_t) g] = (double) (g * (kGrainSize / kNumGrains));
        writePos = 0;

        // Feedback chorus (single modulated delay, dry+wet parallel)
        juce::dsp::ProcessSpec mono { sr, (juce::uint32) blockSize, 1 };
        chorusDelay.prepare (mono);
        chorusDelay.reset();
        chPhase = 0.0;
        chPhaseInc = 0.83 / sr;

        // Haas stereo spread — two short delays panned hard L/R.
        haasL.prepare (mono);
        haasR.prepare (mono);
        haasL.reset();
        haasR.reset();
        haasL.setDelay ((float) (0.011 * sr));
        haasR.setDelay ((float) (0.019 * sr));
    }

    void reset()
    {
        inner.reset();
        fbHpf.reset();
        fbLpf.reset();
        chorusDelay.reset();
        haasL.reset();
        haasR.reset();
        std::fill (pitchBuf.begin(), pitchBuf.end(), 0.0f);
        for (int g = 0; g < kNumGrains; ++g)
            readPos[(size_t) g] = (double) (g * (kGrainSize / kNumGrains));
        writePos = 0;
        chPhase = 0.0;
        prevReverbMono = 0.0f;
    }

    // Adds the shimmer wet output to the given stereo buffer.
    // `wetMix` is the shimmer level (0 = silent, 1 = full).
    // `mainBuffer` carries the main signal to feed *into* the shimmer.
    void processAdd (juce::AudioBuffer<float>& mainBuffer, float wetMix)
    {
        if (wetMix < 1e-4f)
            return;

        const int n   = mainBuffer.getNumSamples();
        const int nch = mainBuffer.getNumChannels();
        auto* L = mainBuffer.getWritePointer (0);
        auto* R = nch > 1 ? mainBuffer.getWritePointer (1) : L;

        constexpr float kFbGain  = 0.75f;
        constexpr float kInGain  = 1.0f;
        constexpr float kChDry   = 0.6f;
        constexpr float kChWet   = 0.4f;
        const     float chBase   = 0.014f * (float) sampleRate;
        const     float chDepth  = 0.0035f * (float) sampleRate;
        constexpr float twoPi    = juce::MathConstants<float>::twoPi;

        // Output panners — pL = -0.8, pR = +0.8 (Web Audio StereoPanner).
        constexpr float panAngL = (-0.8f + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        constexpr float panAngR = ( 0.8f + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        const float pL_L = std::cos (panAngL), pL_R = std::sin (panAngL);
        const float pR_L = std::cos (panAngR), pR_R = std::sin (panAngR);

        // Send-into-shimmer scale matching the standalone (createShimmerReverb's
        // outputG starts at 0, ramped externally). We just multiply by wetMix
        // at the output sum.
        for (int i = 0; i < n; ++i)
        {
            // 1. Feedback path — HPF + LPF + pitch shift on previous reverb out.
            float fb = fbHpf.processSample (prevReverbMono * kFbGain);
            fb       = fbLpf.processSample (fb);
            const float pitched = pitchShiftStep (fb);

            // 2. Chorus (single modulated delay, parallel dry+wet).
            chPhase += chPhaseInc; chPhase -= std::floor (chPhase);
            const float chMod = std::sin ((float) chPhase * twoPi);
            chorusDelay.pushSample (0, pitched);
            chorusDelay.setDelay (chBase + chMod * chDepth);
            const float chOut = chorusDelay.popSample (0);
            const float fbToReverb = pitched * kChDry + chOut * kChWet;

            // 3. Reverb input = main input + feedback contribution.
            const float dryMono = (L[i] + R[i]) * 0.5f;
            const float reverbIn = dryMono * kInGain + fbToReverb;

            float reverbL, reverbR;
            inner.processSampleStereo (reverbIn, reverbL, reverbR);

            prevReverbMono = (reverbL + reverbR) * 0.5f;

            // 4. Haas stereo spread on the dry-tap output.
            haasL.pushSample (0, reverbL);
            haasR.pushSample (0, reverbR);
            const float hL = haasL.popSample (0);
            const float hR = haasR.popSample (0);

            const float wetL = hL * pL_L + hR * pR_L;
            const float wetR = hL * pL_R + hR * pR_R;

            L[i] += wetL * wetMix;
            R[i] += wetR * wetMix;
        }
    }

private:
    // Smaller grains and more overlap = smoother artefacts.
    // 4 Hann-windowed grains at 75% overlap (each offset by N/4): the four
    // staggered Hann functions sum to exactly 2.0 across the cycle, so
    // dividing the output by 2 gives unity gain with no amplitude ripple.
    static constexpr int kPitchBufSize = 4096;   // ~93 ms @ 44.1k
    static constexpr int kGrainSize    = 1024;
    static constexpr int kNumGrains    = 4;

    inline float pitchShiftStep (float in) noexcept
    {
        pitchBuf[(size_t) writePos] = in;

        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        const float invGrain = 1.0f / (float) kGrainSize;

        float out = 0.0f;
        for (int g = 0; g < kNumGrains; ++g)
        {
            const float w = 0.5f * (1.0f - std::cos (twoPi * (float) readPos[g] * invGrain));

            // Linear-interpolated read at fractional position behind the write head.
            const double exact = (double) writePos - (double) kGrainSize + readPos[g];
            const double exactWrapped = exact - std::floor (exact / (double) kPitchBufSize)
                                              * (double) kPitchBufSize;
            const int   idx0 = (int) exactWrapped;
            const int   idx1 = (idx0 + 1) % kPitchBufSize;
            const float frac = (float) (exactWrapped - (double) idx0);
            const float s    = pitchBuf[(size_t) idx0] * (1.0f - frac)
                             + pitchBuf[(size_t) idx1] * frac;

            out += s * w;

            readPos[g] += 2.0;       // 2× rate ⇒ +12 semitones
            if (readPos[g] >= (double) kGrainSize)
                readPos[g] -= (double) kGrainSize;
        }

        writePos = (writePos + 1) % kPitchBufSize;
        return out * 0.5f;           // normalise sum-of-Hann at 75% overlap
    }

    double sampleRate = 44100.0;

    DattorroReverb inner;
    juce::dsp::IIR::Filter<float> fbHpf, fbLpf;

    std::vector<float> pitchBuf;
    std::array<double, 4> readPos { 0.0, 256.0, 512.0, 768.0 };
    int writePos = 0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> chorusDelay { 4096 };
    double chPhase = 0.0, chPhaseInc = 0.0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> haasL { 4096 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> haasR { 4096 };

    float prevReverbMono = 0.0f;
};
