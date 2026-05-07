#pragma once

#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "DattorroReverb.h"
#include "GranularPitchShift.h"

// Shimmer v4 — proper feedback-loop shimmer reverb.
//
// Topology (per block):
//   dry input + previous-block feedback   →  Dattorro plate reverb
//   reverb out                            →  HPF · LPF · +12 st pitch
//                                           shift · tanh sat · gain
//                                         →  becomes next block's feedback
//   reverb out (pre-feedback-tap)         →  user wet
//
// Sparkle is the reverb tail itself recursively pitched up an octave —
// the same trick Eventide / Valhalla / Strymon use. Per-block feedback
// adds ~1 block of latency in the loop (≈3-12 ms at typical sizes),
// which is musically inaudible but lets the granular shifter run
// block-rate where it sounds best.
//
// Pitch shifter is from Danial Kooshki's MIT-licensed GranularPitchShift
// (see GranularPitchShift.h for full attribution).
class Shimmer
{
public:
    void prepare (double sr, int blockSize)
    {
        sampleRate = sr;
        maxBlock   = juce::jmax (16, blockSize);

        inner.prepare (sr, blockSize);
        inner.setBandwidth        (0.9999f);
        inner.setDamping          (0.04f);
        inner.setDecay            (0.85f);
        inner.setExcursionRate    (0.35f);
        inner.setExcursionDepthMs (0.6f);
        inner.setPreDelaySamples  ((int) (0.005 * sr));
        inner.reset();

        const auto hpf = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr,  180.0f, 0.5f);
        const auto lpf = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, 6500.0f, 0.5f);
        *fbHpfL.coefficients = *hpf; *fbHpfR.coefficients = *hpf;
        *fbLpfL.coefficients = *lpf; *fbLpfR.coefficients = *lpf;
        fbHpfL.reset(); fbHpfR.reset();
        fbLpfL.reset(); fbLpfR.reset();

        shifter.prepare (sr, /*channels*/ 2, blockSize, /*grainMs*/ 100, /*voices*/ 8, /*jitterMs*/ 5.0f);
        shifter.setSemitones (12.0f);    // octave up
        shifter.reset();

        wetBuf .setSize (2, maxBlock, false, false, true);
        loopBuf.setSize (2, maxBlock, false, false, true);
        fbBuf  .setSize (2, maxBlock, false, false, true);
        wetBuf.clear(); loopBuf.clear(); fbBuf.clear();
    }

    void reset()
    {
        inner.reset();
        fbHpfL.reset(); fbHpfR.reset();
        fbLpfL.reset(); fbLpfR.reset();
        shifter.reset();
        wetBuf.clear(); loopBuf.clear(); fbBuf.clear();
    }

    // Adds the shimmer wet output onto mainBuffer. wetMix scales the
    // octave-up reverb send into the main bus.
    void processAdd (juce::AudioBuffer<float>& mainBuffer, float wetMix)
    {
        if (wetMix < 1e-4f) return;

        const int n   = mainBuffer.getNumSamples();
        const int nch = mainBuffer.getNumChannels();
        if (n > wetBuf.getNumSamples())
        {
            wetBuf .setSize (2, n, false, false, true);
            loopBuf.setSize (2, n, false, false, true);
            fbBuf  .setSize (2, n, false, false, true);
        }

        auto* L = mainBuffer.getWritePointer (0);
        auto* R = nch > 1 ? mainBuffer.getWritePointer (1) : L;

        auto* wetL = wetBuf.getWritePointer (0);
        auto* wetR = wetBuf.getWritePointer (1);

        // 1. Reverb stage: input = dry + previous block's feedback tail.
        const float* fbPrevL = fbBuf.getReadPointer (0);
        const float* fbPrevR = fbBuf.getReadPointer (1);

        for (int i = 0; i < n; ++i)
        {
            const float monoIn = (L[i] + R[i]) * 0.5f
                               + (fbPrevL[i] + fbPrevR[i]) * 0.5f * kFeedbackGain;
            inner.processSampleStereo (monoIn, wetL[i], wetR[i]);
        }

        // 2. Build next block's feedback tail from this block's wet:
        //    wet → HPF → LPF → +12 st pitch shift → tanh saturation.
        loopBuf.copyFrom (0, 0, wetBuf, 0, 0, n);
        loopBuf.copyFrom (1, 0, wetBuf, 1, 0, n);
        {
            auto* lL = loopBuf.getWritePointer (0);
            auto* lR = loopBuf.getWritePointer (1);
            for (int i = 0; i < n; ++i)
            {
                lL[i] = fbHpfL.processSample (lL[i]);
                lR[i] = fbHpfR.processSample (lR[i]);
                lL[i] = fbLpfL.processSample (lL[i]);
                lR[i] = fbLpfR.processSample (lR[i]);
            }
        }

        // The granular shifter is block-based, so we hand it a sub-buffer
        // of size n that aliases the loopBuf storage.
        juce::AudioBuffer<float> loopIn  (loopBuf.getArrayOfWritePointers(), 2, n);
        juce::AudioBuffer<float> loopOut (fbBuf  .getArrayOfWritePointers(), 2, n);
        loopOut.clear();
        shifter.processBlock (loopIn, loopOut);

        {
            auto* fL = fbBuf.getWritePointer (0);
            auto* fR = fbBuf.getWritePointer (1);
            for (int i = 0; i < n; ++i)
            {
                fL[i] = std::tanh (fL[i] * 1.5f) * 0.85f;
                fR[i] = std::tanh (fR[i] * 1.5f) * 0.85f;
            }
        }

        // 3. Sum wet into the main bus.
        const float send = wetMix * 1.0f;
        for (int i = 0; i < n; ++i)
        {
            L[i] += wetL[i] * send;
            R[i] += wetR[i] * send;
        }
    }

private:
    static constexpr float kFeedbackGain = 0.55f;   // pre-tanh feedback level

    double sampleRate = 44100.0;
    int    maxBlock   = 512;

    DattorroReverb inner;
    juce::dsp::IIR::Filter<float> fbHpfL, fbHpfR, fbLpfL, fbLpfR;
    GranularPitchShift shifter;

    juce::AudioBuffer<float> wetBuf;     // reverb output for this block
    juce::AudioBuffer<float> loopBuf;    // wet → filters → pitch shift staging
    juce::AudioBuffer<float> fbBuf;      // pitch-shifted feedback for next block
};
