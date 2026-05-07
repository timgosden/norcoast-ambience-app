#pragma once
//
// GranularPitchShift — multi-voice granular pitch shifter.
//
// Adapted verbatim from Danial Kooshki's Shimmer-Reverb plugin
// (https://github.com/Danial-k-f/Shimmer-Reverb), distributed under the
// MIT licence. Original copyright header preserved below.
//
//   MIT License
//   Copyright (c) 2025 Danial Kooshki
//   Permission is hereby granted, free of charge, to any person obtaining
//   a copy of this software and associated documentation files (the
//   "Software"), to deal in the Software without restriction, including
//   without limitation the rights to use, copy, modify, merge, publish,
//   distribute, sublicense, and/or sell copies of the Software, and to
//   permit persons to whom the Software is furnished to do so, subject
//   to the following conditions:
//
//   The above copyright notice and this permission notice shall be
//   included in all copies or substantial portions of the Software.
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
//
// Substituted juce::AudioBuffer / juce::dsp / std::random for the
// JuceHeader.h include since this project doesn't ship JuceHeader.

#include <vector>
#include <random>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

class GranularPitchShift
{
public:
    void prepare (double sampleRate, int channels, int blockSize,
                  int grainMs = 100, int voices = 8, float jitterMs = 5.0f)
    {
        fs = sampleRate;
        ch = juce::jmax (1, channels);
        bs = blockSize;

        setGrainMs (grainMs);
        setVoices  (voices);
        setJitterMs (jitterMs);

        const int minLookback = grainSize;
        lookbackDelay = juce::jmax (grainSize, minLookback);
        circ.setSize (ch, grainSize * 4 + lookbackDelay + 8);
        circ.clear();
        writePos = 0;

        window.resize ((size_t) grainSize);
        for (int i = 0; i < grainSize; ++i)
            window[(size_t) i] = (float)
                (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi
                                       * (double) i / (double) grainSize));

        rng.seed (0xC0FFEE);
        voicePhase.assign ((size_t) numVoices, 0.0);
        voiceAge  .assign ((size_t) numVoices, 0);
        voiceDelay.assign ((size_t) numVoices, lookbackDelay);
        nextTrig = 0;
        hopSize = juce::jmax (1, grainSize / 3);

        setSemitones (semitones);
    }

    void reset()
    {
        circ.clear();
        writePos = 0;
        std::fill (voicePhase.begin(), voicePhase.end(), 0.0);
        std::fill (voiceAge  .begin(), voiceAge  .end(), 0);
        std::fill (voiceDelay.begin(), voiceDelay.end(), lookbackDelay);
        nextTrig = 0;
        zPrev = semitones;
        pitchRatioSmoothed = pitchRatio;
    }

    void setSemitones (float st)
    {
        semitones  = st;
        pitchRatio = std::pow (2.0, (double) st / 12.0);
    }

    void setGrainMs (int ms)
    {
        grainSize = juce::jmax (32, (int) std::round ((double) ms * fs * 0.001));
    }

    void setVoices (int v)
    {
        numVoices = juce::jlimit (2, 16, v);
    }

    void setJitterMs (float ms)
    {
        jitterSamples = (int) std::round (ms * (float) fs * 0.001f);
    }

    void processBlock (const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out)
    {
        const int n  = in.getNumSamples();
        const int cc = juce::jmin (ch, in.getNumChannels(), out.getNumChannels());
        jassert (out.getNumSamples() == n);

        const float zTarget = semitones;
        const float a = 0.995f;
        zPrev = a * zPrev + (1.0f - a) * zTarget;
        pitchRatio = std::pow (2.0, (double) zPrev / 12.0);
        pitchRatioSmoothed = a * pitchRatioSmoothed + (1.0 - a) * pitchRatio;

        const int circSize = circ.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < cc; ++c)
                circ.setSample (c, writePos, in.getSample (c, i));

            for (int c = 0; c < cc; ++c)
                out.setSample (c, i, 0.0f);

            for (int v = 0; v < numVoices; ++v)
            {
                if (voiceAge[(size_t) v] <= 0)
                {
                    voiceAge  [(size_t) v] = grainSize;
                    voicePhase[(size_t) v] = 0.0;
                    int j = (jitterSamples > 0
                             ? (int) std::uniform_int_distribution<int> (-jitterSamples,
                                                                          jitterSamples) (rng)
                             : 0);
                    voiceDelay[(size_t) v] = lookbackDelay + j;
                }

                double ph   = voicePhase[(size_t) v];
                int    ip   = (int) ph;
                float  frac = (float) (ph - ip);

                float env = window[(size_t) (ip % grainSize)];

                int readIndex  = writePos - voiceDelay[(size_t) v] + ip;
                int readIndex2 = readIndex + 1;
                while (readIndex  < 0) readIndex  += circSize;
                while (readIndex2 < 0) readIndex2 += circSize;
                readIndex  %= circSize;
                readIndex2 %= circSize;

                for (int c = 0; c < cc; ++c)
                {
                    float s1 = circ.getSample (c, readIndex);
                    float s2 = circ.getSample (c, readIndex2);
                    float s  = s1 * (1.0f - frac) + s2 * frac;
                    float acc = out.getSample (c, i);
                    out.setSample (c, i, acc + s * env / (float) numVoices);
                }

                voicePhase[(size_t) v] += pitchRatioSmoothed;
                voiceAge  [(size_t) v] -= 1;

                if (voicePhase[(size_t) v] >= grainSize)
                    voicePhase[(size_t) v] -= grainSize;
            }

            writePos = (writePos + 1) % circSize;

            ++nextTrig;
            if (nextTrig >= hopSize)
            {
                nextTrig = 0;
                int minIdx = 0;
                int minAge = voiceAge[0];
                for (int v = 1; v < numVoices; ++v)
                    if (voiceAge[(size_t) v] < minAge)
                    {
                        minAge = voiceAge[(size_t) v];
                        minIdx = v;
                    }
                voiceAge[(size_t) minIdx] = 0;
            }
        }
    }

private:
    double fs = 48000.0;
    int    ch = 2;
    int    bs = 512;

    int   grainSize     = 4096;
    int   numVoices     = 6;
    int   hopSize       = 0;
    int   lookbackDelay = 0;
    int   jitterSamples = 0;

    juce::AudioBuffer<float> circ;
    int writePos = 0;

    std::vector<float>  window;
    std::vector<double> voicePhase;
    std::vector<int>    voiceAge;
    std::vector<int>    voiceDelay;
    int nextTrig = 0;

    float  semitones = 0.0f, zPrev = 0.0f;
    double pitchRatio = 1.0, pitchRatioSmoothed = 1.0;

    std::mt19937 rng { 0xC0FFEE };
};
