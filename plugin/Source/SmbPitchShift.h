#pragma once
//
// SmbPitchShift — phase-vocoder pitch shifter.
//
// Algorithm follows Stephan M. Bernsee's classic 1999 STFT pitch
// shifter (originally posted at dspdimension.com, and the basis for
// most software shimmer reverbs since). Re-bins the magnitude
// spectrum and reconstructs phases from per-bin true-frequency
// estimates — no grain boundaries, so +12 st on a polyphonic / smeared
// reverb tail comes out clean rather than warbly.
//
// Latency: kFftSize - kHopSize samples (= 1536 at the default
// 2048/4 setup ≈ 32 ms at 48 kHz). Acceptable for a parallel
// shimmer wet send sitting after the main reverb.
//
// Implementation written from scratch against juce::dsp::FFT — the
// algorithm is well-known and not patented, but we credit Bernsee
// for the technique.

#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

class SmbPitchShift
{
public:
    void prepare (double sr, int channels, int /*blockSize*/)
    {
        sampleRate = sr;
        const int nch = juce::jmax (1, channels);

        // FFT object — shared across channels (it's stateless once
        // constructed, only the per-channel FIFO/phase buffers matter).
        fft = std::make_unique<juce::dsp::FFT> (kFftOrder);

        chanState.assign ((size_t) nch, ChannelState());
        for (auto& s : chanState)
            s.reset();

        // Hann window (precomputed once; the same shape is used for
        // analysis and synthesis).
        for (int k = 0; k < kFftSize; ++k)
            window[(size_t) k] = -0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                   * (float) k / (float) kFftSize)
                                 + 0.5f;
    }

    void reset()
    {
        for (auto& s : chanState) s.reset();
    }

    void setSemitones (float st) noexcept
    {
        pitchScale = std::pow (2.0f, st / 12.0f);
    }

    // Process a multi-channel block. The output buffer is OVERWRITTEN
    // (not added) — the caller can mix it into the main bus afterwards.
    void processBlock (const juce::AudioBuffer<float>& in,
                       juce::AudioBuffer<float>& out)
    {
        const int n   = juce::jmin (in.getNumSamples(), out.getNumSamples());
        const int nch = juce::jmin (in.getNumChannels(),
                                     juce::jmin (out.getNumChannels(), (int) chanState.size()));
        for (int c = 0; c < nch; ++c)
        {
            const float* src = in.getReadPointer (c);
            float*       dst = out.getWritePointer (c);
            auto& s = chanState[(size_t) c];
            for (int i = 0; i < n; ++i)
                dst[i] = processSample (s, src[i]);
        }
    }

private:
    static constexpr int kFftOrder = 11;             // 2048 = 2^11
    static constexpr int kFftSize  = 1 << kFftOrder; // 2048
    static constexpr int kOversamp = 4;
    static constexpr int kHopSize  = kFftSize / kOversamp; // 512
    static constexpr int kHalf     = kFftSize / 2;
    static constexpr int kLatency  = kFftSize - kHopSize;  // 1536

    struct ChannelState
    {
        std::array<float, kFftSize>     inFifo  {};
        std::array<float, kFftSize>     outFifo {};
        std::array<float, kFftSize * 2> outAccum {};
        std::array<float, kHalf + 1>    lastPhase {};
        std::array<float, kHalf + 1>    sumPhase  {};
        std::array<std::complex<float>, kFftSize> fftIn  {};
        std::array<std::complex<float>, kFftSize> fftOut {};
        std::array<float, kHalf + 1>    magBuf {};
        std::array<float, kHalf + 1>    freqBuf {};
        std::array<float, kHalf + 1>    synMag {};
        std::array<float, kHalf + 1>    synFreq {};
        int rover = 0;

        void reset()
        {
            inFifo .fill (0.0f);
            outFifo.fill (0.0f);
            outAccum.fill (0.0f);
            lastPhase.fill (0.0f);
            sumPhase .fill (0.0f);
            magBuf  .fill (0.0f);
            freqBuf .fill (0.0f);
            synMag  .fill (0.0f);
            synFreq .fill (0.0f);
            rover = kLatency;
        }
    };

    float processSample (ChannelState& s, float in)
    {
        // 1. FIFO write/read at the rover pointer. The output sample
        //    is read from outFifo[rover - kLatency] which advances 1:1
        //    with the input sample stream.
        s.inFifo[(size_t) s.rover] = in;
        const float out = s.outFifo[(size_t) (s.rover - kLatency)];
        ++s.rover;

        if (s.rover < kFftSize)
            return out;

        // 2. Reset rover so the next input sample starts at kLatency.
        s.rover = kLatency;

        // 3. Window + FFT (forward).
        for (int k = 0; k < kFftSize; ++k)
        {
            s.fftIn[(size_t) k] = std::complex<float> (
                s.inFifo[(size_t) k] * window[(size_t) k], 0.0f);
        }
        fft->perform (s.fftIn.data(), s.fftOut.data(), false);

        // 4. Analysis — compute magnitude + true frequency per bin.
        const float expectedPhase = 2.0f * juce::MathConstants<float>::pi
                                    * (float) kHopSize / (float) kFftSize;
        const float freqPerBin    = (float) sampleRate / (float) kFftSize;

        for (int k = 0; k <= kHalf; ++k)
        {
            const float real = s.fftOut[(size_t) k].real();
            const float imag = s.fftOut[(size_t) k].imag();
            const float magn = 2.0f * std::sqrt (real * real + imag * imag);
            const float phase = std::atan2 (imag, real);

            // Phase delta vs last frame, with the bin's expected phase
            // advance subtracted out.
            float pd = phase - s.lastPhase[(size_t) k];
            s.lastPhase[(size_t) k] = phase;
            pd -= (float) k * expectedPhase;

            // Wrap to [-π, π].
            int qpd = (int) (pd / juce::MathConstants<float>::pi);
            if (qpd >= 0) qpd += qpd & 1;
            else          qpd -= qpd & 1;
            pd -= juce::MathConstants<float>::pi * (float) qpd;

            // Convert to a fractional bin offset (oversamp scaling).
            pd = (float) kOversamp * pd
                 / (2.0f * juce::MathConstants<float>::pi);

            s.magBuf [(size_t) k] = magn;
            s.freqBuf[(size_t) k] = ((float) k + pd) * freqPerBin;
        }

        // 5. Re-bin: for each input bin, deposit its magnitude at the
        //    pitch-scaled bin index and store the scaled true freq.
        std::fill (s.synMag .begin(), s.synMag .end(), 0.0f);
        std::fill (s.synFreq.begin(), s.synFreq.end(), 0.0f);
        for (int k = 0; k <= kHalf; ++k)
        {
            const int newK = (int) ((float) k * pitchScale + 0.5f);
            if (newK <= kHalf)
            {
                s.synMag [(size_t) newK] += s.magBuf [(size_t) k];
                s.synFreq[(size_t) newK]  = s.freqBuf[(size_t) k] * pitchScale;
            }
        }

        // 6. Synthesis — convert each bin's true frequency back to a
        //    phase delta, accumulate into sumPhase, build the complex
        //    spectrum.
        for (int k = 0; k <= kHalf; ++k)
        {
            const float magn = s.synMag[(size_t) k];
            float tmp = s.synFreq[(size_t) k];
            tmp = tmp / freqPerBin - (float) k;
            tmp = 2.0f * juce::MathConstants<float>::pi * tmp / (float) kOversamp;
            tmp += (float) k * expectedPhase;
            s.sumPhase[(size_t) k] += tmp;
            const float ph = s.sumPhase[(size_t) k];
            s.fftIn[(size_t) k] = std::complex<float> (magn * std::cos (ph),
                                                       magn * std::sin (ph));
        }
        // Mirror upper half (negative freqs) for the inverse FFT.
        for (int k = kHalf + 1; k < kFftSize; ++k)
            s.fftIn[(size_t) k] = std::conj (s.fftIn[(size_t) (kFftSize - k)]);

        // 7. Inverse FFT.
        fft->perform (s.fftIn.data(), s.fftOut.data(), true);

        // 8. Window + overlap-add into outAccum.
        //    Scale factor accounts for the analysis-magnitude doubling
        //    (×2 because we built the spectrum from positive freqs and
        //    mirrored to negative freqs) and the windowed-OLA gain
        //    (Hann sum-of-squares ≈ 1.5 at osamp=4). JUCE's IFFT is
        //    already normalised by 1/N so we don't divide by N again.
        const float scale = 2.0f / (float) kOversamp;
        for (int k = 0; k < kFftSize; ++k)
            s.outAccum[(size_t) k] += window[(size_t) k]
                                       * s.fftOut[(size_t) k].real()
                                       * scale;

        // 9. Drop the next kHopSize output samples into outFifo[0..hop)
        //    so the per-sample read at the top of processSample picks
        //    them up.
        for (int k = 0; k < kHopSize; ++k)
            s.outFifo[(size_t) k] = s.outAccum[(size_t) k];

        // 10. Slide outAccum left by kHopSize, zero the tail.
        std::memmove (s.outAccum.data(),
                      s.outAccum.data() + kHopSize,
                      sizeof (float) * (size_t) (kFftSize * 2 - kHopSize));
        std::fill (s.outAccum.begin() + (kFftSize * 2 - kHopSize),
                   s.outAccum.end(), 0.0f);

        // 11. Slide inFifo left by kHopSize so the next batch of fresh
        //     samples gets written at indices [kLatency..kFftSize).
        std::memmove (s.inFifo.data(),
                      s.inFifo.data() + kHopSize,
                      sizeof (float) * (size_t) (kFftSize - kHopSize));

        return out;
    }

    double sampleRate = 44100.0;
    float  pitchScale = 1.0f;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::array<float, kFftSize>     window {};
    std::vector<ChannelState>       chanState;
};
