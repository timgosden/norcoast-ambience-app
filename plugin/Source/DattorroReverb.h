#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

// Direct C++ port of the standalone's Dattorro plate reverb worklet
// (public/dattorro-reverb-worklet.js). Algorithm: Jon Dattorro,
// "Effect Design Part 1: Reverberator and Other Filters"
// (JAES Vol. 45, No. 9, 1997 September), via khoin/DattorroReverbNode.
//
// 12 delay lines + pre-delay buffer + 3 one-pole LPFs + two excursion
// taps with cubic interpolation. Stereo output is built from 14
// hard-coded tap positions across the decay loop.
//
// The worklet's wet/dry mix is bypassed here — we run the algorithm
// "wet only" and the host code (PluginProcessor) does the parallel
// mix with a configurable wetLevel/dryLevel pair.
class DattorroReverb
{
public:
    void prepare (double sampleRateIn, int /*blockSize*/)
    {
        sampleRate = sampleRateIn;

        // Pre-delay buffer: ~1 s, rounded up to a multiple of 128 to
        // match the worklet's behaviour exactly.
        preDelayLen = (int) sampleRate + (128 - ((int) sampleRate) % 128);
        preDelay.assign ((size_t) preDelayLen, 0.0f);
        preDelayWrite = 0;

        // Delay-line lengths in seconds — verbatim from the worklet.
        constexpr std::array<double, kNumDelays> delaySecs
        {
            0.004771345, 0.003595309, 0.012734787, 0.009307483,
            0.022579886, 0.149625349, 0.060481839, 0.1249958,
            0.030509727, 0.141695508, 0.089244313, 0.106280031
        };
        for (int i = 0; i < kNumDelays; ++i)
            makeDelay (delays[(size_t) i], delaySecs[(size_t) i]);

        // Stereo output tap positions (seconds → samples).
        constexpr std::array<double, 14> tapSecs
        {
            0.008937872, 0.099929438, 0.064278754, 0.067067639, 0.066866033,
            0.006283391, 0.035818689, 0.011861161, 0.121870905, 0.041262054,
            0.08981553,  0.070931756, 0.011256342, 0.004065724
        };
        for (int i = 0; i < 14; ++i)
            taps[(size_t) i] = (int) std::round (tapSecs[(size_t) i] * sampleRate);

        excPhase = 0.0;
    }

    void reset()
    {
        std::fill (preDelay.begin(), preDelay.end(), 0.0f);
        for (auto& d : delays)
            std::fill (d.buf.begin(), d.buf.end(), 0.0f);
        lp1 = lp2 = lp3 = 0.0f;
        excPhase = 0.0;
        preDelayWrite = 0;
    }

    // Setters — mirror the standalone's parameter mappings.
    void setBandwidth        (float v) { bandwidth = v; }
    void setInputDiffusion1  (float v) { diff1 = v; }
    void setInputDiffusion2  (float v) { diff2 = v; }
    void setDecay            (float v) { decay = v; }
    void setDecayDiffusion1  (float v) { decayDiff1 = v; }
    void setDecayDiffusion2  (float v) { decayDiff2 = v; }
    void setDamping          (float v) { dampingInv = 1.0f - v; }
    void setExcursionRate    (float hz)         { excursionRate = hz; }
    void setExcursionDepthMs (float depthMs)
    {
        excursionDepthSamp = depthMs * (float) sampleRate / 1000.0f;
    }
    void setPreDelaySamples  (int n) { preDelaySamples = n; }

    // Replaces buffer contents with the wet-only reverb output.
    // Caller is responsible for any wet/dry mixing.
    void processWet (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int nch = buffer.getNumChannels();
        auto* L = buffer.getWritePointer (0);
        auto* R = nch > 1 ? buffer.getWritePointer (1) : L;

        for (int i = 0; i < n; ++i)
        {
            // Mono sum into the pre-delay buffer.
            preDelay[(size_t) preDelayWrite] = (L[i] + R[i]) * 0.5f;

            const int pdRead = (preDelayLen + preDelayWrite - preDelaySamples)
                               % preDelayLen;
            preDelayWrite = (preDelayWrite + 1) % preDelayLen;

            lp1 += bandwidth * (preDelay[(size_t) pdRead] - lp1);

            // Input diffusion: 4 allpasses chained.
            float pre = writeDelay (0, lp1 - diff1 * readDelay (0));
            pre       = writeDelay (1, diff1 * (pre - readDelay (1)) + readDelay (0));
            pre       = writeDelay (2, diff1 * pre + readDelay (1) - diff2 * readDelay (2));
            pre       = writeDelay (3, diff2 * (pre - readDelay (3)) + readDelay (2));
            const float split = diff2 * pre + readDelay (3);

            // Modulated decay loop: two parallel "tanks" cross-coupled
            // via the decay coefficient. Excursion uses cubic interp to
            // dodge zipper artefacts on the fractional read.
            const float exc  = excursionDepthSamp
                             * (1.0f + std::cos ((float) excPhase * 6.2800f));
            const float exc2 = excursionDepthSamp
                             * (1.0f + std::sin ((float) excPhase * 6.2847f));

            float temp = writeDelay (4, split + decay * readDelay (11)
                                     + decayDiff1 * readDelayCubic (4, exc));
            writeDelay (5, readDelayCubic (4, exc) - decayDiff1 * temp);
            lp2 += dampingInv * (readDelay (5) - lp2);
            temp = writeDelay (6, decay * lp2 - decayDiff2 * readDelay (6));
            writeDelay (7, readDelay (6) + decayDiff2 * temp);

            temp = writeDelay (8, split + decay * readDelay (7)
                                + decayDiff1 * readDelayCubic (8, exc2));
            writeDelay (9, readDelayCubic (8, exc2) - decayDiff1 * temp);
            lp3 += dampingInv * (readDelay (9) - lp3);
            temp = writeDelay (10, decay * lp3 - decayDiff2 * readDelay (10));
            writeDelay (11, readDelay (10) + decayDiff2 * temp);

            const float lo = readDelayAt (9,  taps[0])  + readDelayAt (9,  taps[1])
                           - readDelayAt (10, taps[2])  + readDelayAt (11, taps[3])
                           - readDelayAt (5,  taps[4])  - readDelayAt (6,  taps[5])
                           - readDelayAt (7,  taps[6]);
            const float ro = readDelayAt (5,  taps[7])  + readDelayAt (5,  taps[8])
                           - readDelayAt (6,  taps[9])  + readDelayAt (7,  taps[10])
                           - readDelayAt (9,  taps[11]) - readDelayAt (10, taps[12])
                           - readDelayAt (11, taps[13]);

            // Worklet's internal *0.6 wet scale — keeps headroom.
            L[i] = lo * 0.6f;
            R[i] = ro * 0.6f;

            excPhase += (double) excursionRate / sampleRate;
            if (excPhase > 1024.0)
                excPhase -= 1024.0;

            for (auto& d : delays)
            {
                d.writeIdx = (d.writeIdx + 1) & d.mask;
                d.readIdx  = (d.readIdx  + 1) & d.mask;
            }
        }
    }

private:
    static constexpr int kNumDelays = 12;

    struct Delay
    {
        std::vector<float> buf;
        int writeIdx = 0;
        int readIdx  = 0;
        int mask     = 0;
    };

    void makeDelay (Delay& d, double seconds)
    {
        const int len = (int) std::round (seconds * sampleRate);
        // Round up to next power of two so we can use bitmask wrap.
        int pow2 = 1;
        while (pow2 < len) pow2 <<= 1;
        d.buf.assign ((size_t) pow2, 0.0f);
        d.writeIdx = len - 1;
        d.readIdx  = 0;
        d.mask     = pow2 - 1;
    }

    float writeDelay (int i, float x)
    {
        auto& d = delays[(size_t) i];
        d.buf[(size_t) d.writeIdx] = x;
        return x;
    }

    float readDelay (int i) const
    {
        const auto& d = delays[(size_t) i];
        return d.buf[(size_t) d.readIdx];
    }

    float readDelayAt (int i, int offset) const
    {
        const auto& d = delays[(size_t) i];
        return d.buf[(size_t) ((d.readIdx + offset) & d.mask)];
    }

    // Cubic interpolation: 4-point Hermite (Catmull-Rom). Matches the
    // worklet's readDelayCAt exactly — same coefficient form.
    float readDelayCubic (int i, float fIdx) const
    {
        const auto& d = delays[(size_t) i];
        const int   floored = (int) fIdx;
        const float frac    = fIdx - (float) floored;
        const int   mask    = d.mask;
        int idx             = floored + d.readIdx - 1;

        const float x0 = d.buf[(size_t) (idx++ & mask)];
        const float x1 = d.buf[(size_t) (idx++ & mask)];
        const float x2 = d.buf[(size_t) (idx++ & mask)];
        const float x3 = d.buf[(size_t) (idx   & mask)];
        const float a  = (3.0f * (x1 - x2) - x0 + x3) * 0.5f;
        const float b  = 2.0f * x2 + x0 - (5.0f * x1 + x3) * 0.5f;
        const float c  = (x2 - x0) * 0.5f;
        return (((a * frac) + b) * frac + c) * frac + x1;
    }

    double sampleRate = 44100.0;

    std::vector<float> preDelay;
    int  preDelayLen   = 0;
    int  preDelayWrite = 0;
    int  preDelaySamples = 0;

    std::array<Delay, kNumDelays> delays;
    std::array<int, 14> taps {};

    float lp1 = 0.0f, lp2 = 0.0f, lp3 = 0.0f;
    double excPhase = 0.0;

    // Defaults from the standalone (init in createDattorroReverb).
    float bandwidth     = 0.9995f;
    float diff1         = 0.75f;
    float diff2         = 0.625f;
    float decay         = 0.829f;     // reverbSizeToFB(0.92)
    float decayDiff1    = 0.7f;
    float decayDiff2    = 0.5f;
    float dampingInv    = 1.0f - 0.005f;
    float excursionRate = 1.558f;     // 0.3 + 0.74 * 1.7  (reverbMod = 0.74)
    float excursionDepthSamp = 0.0f;  // set in prepare() via setExcursionDepthMs
};
