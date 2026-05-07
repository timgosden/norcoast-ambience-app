#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>

// Step-sequencer drum machine ported from public/index.html's
// fireMovementHit + MOVEMENT_PATTERNS. Three sounds:
//   k - 808-style kick     (sine pitch sweep 145 → 48 → 40 Hz)
//   t - 808-style tom      (sine pitch sweep 260 → 110 → 95 Hz)
//   h - lofi shaker hat    (HP+LP filtered noise, 80 ms decay)
// Patterns are 16-step bars in 4/4. Tempo is read from the host
// playhead by the caller and passed in each block.
class DrumMachine
{
public:
    enum class PatternId { Off = 0, Pulse, Mist, Stride, Roam, Custom, NumPatterns };

    void prepare (double sr, int /*blockSize*/)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        lastFiredStep = -1;
        kickAge = tomAge = hatAge = -1;
        kickPhase = tomPhase = 0.0;
        hatHp = hatLp = 0.0f;
    }

    // Adds drum audio onto `buffer`. `vol` is master drum level
    // (0 = silent), `patternId` selects the pattern, `bpm` from host.
    // For Custom patterns, pass the 16-bit masks (lo/md/hh) — bit i set
    // means that voice fires on step i.
    // `timeSig` is 0 = 4/4 (16 steps, step = 1/16), 1 = 6/8 (12 steps,
    // step = 1/8 of dotted-quarter beat).
    // `transportSamples` is the shared block-start sample clock so the
    // drum step grid is locked to the same transport as arp + evolve.
    void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                  float vol, int patternId, double bpm,
                  int customLo, int customMd, int customHh,
                  int timeSig, juce::int64 transportSamples)
    {
        const bool muted = vol < 1e-4f || patternId <= 0
                           || patternId >= (int) PatternId::NumPatterns;

        const bool is68    = timeSig == 1;
        const int  numSteps = is68 ? 12 : 16;
        // 4/4: step = 1/16 = 0.25 beats. 6/8: step = 1/8 (eighth-note) = 0.5 beats.
        const double stepBeats = is68 ? 0.5 : 0.25;
        const int  stepDur     = juce::jmax (1, (int) ((60.0 / bpm) * stepBeats * sampleRate));
        const bool isCustom    = patternId == (int) PatternId::Custom;

        auto* L = buffer.getWritePointer (0) + startSample;
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) + startSample : L;

        for (int s = 0; s < numSamples; ++s)
        {
            const juce::int64 stepNow = (transportSamples + s) / stepDur;
            if (! muted && stepNow != lastFiredStep)
            {
                const int idx = (int) (stepNow % numSteps);
                const int stepBit = 1 << idx;
                juce::uint8 stepCode = 0;
                if (isCustom)
                {
                    if (customLo & stepBit) stepCode |= MaskKick;
                    if (customMd & stepBit) stepCode |= MaskTom;
                    if (customHh & stepBit) stepCode |= MaskHat;
                }
                else
                {
                    stepCode = is68
                        ? kPatterns68[(size_t) patternId][(size_t) idx]
                        : kPatterns[(size_t) patternId][(size_t) idx];
                }
                if (stepCode != 0)
                {
                    if (stepCode & MaskKick) triggerKick();
                    if (stepCode & MaskTom)  triggerTom();
                    if (stepCode & MaskHat)  triggerHat();
                }
                lastFiredStep = stepNow;
            }

            float l = 0.0f, r = 0.0f;
            renderKick (l, r);
            renderTom  (l, r);
            renderHat  (l, r);

            if (! muted)
            {
                L[s] += l * vol;
                R[s] += r * vol;
            }
        }

        // Keep step counter ticking even when muted so re-enabling locks back
        // to the master grid instead of starting fresh from the press moment.
        if (muted)
            lastFiredStep = (transportSamples + numSamples - 1) / stepDur;
    }

private:
    static constexpr juce::uint8 MaskKick = 0x1;
    static constexpr juce::uint8 MaskTom  = 0x2;
    static constexpr juce::uint8 MaskHat  = 0x4;

    // Patterns are 16 steps each; bitmask of sounds firing on that step.
    // The Custom slot is a zeroed placeholder — process() reads the
    // user's mask params instead of this entry when patternId = Custom.
    static constexpr std::array<std::array<juce::uint8, 16>, (size_t) PatternId::NumPatterns> kPatterns
    {{
        // Off
        {{0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}},
        // Pulse: tom on beats 2 & 4 (steps 4 & 12)
        {{0,0,0,0, MaskTom,0,0,0, 0,0,0,0, MaskTom,0,0,0}},
        // Mist: hihats on a flowing pattern
        {{MaskHat,0,0,MaskHat, 0,0,MaskHat,0, 0,0,0,MaskHat, 0,0,0,0}},
        // Stride: kick on 1, tom on 2 & 4
        {{MaskKick,0,0,0, MaskTom,0,0,0, 0,0,0,0, MaskTom,0,0,0}},
        // Roam: kick + hat layered with tom and extra hats
        {{MaskKick|MaskHat,0,0,MaskHat, MaskTom,0,MaskHat,0,
          0,0,0,MaskHat, MaskTom,0,0,0}},
        // Custom: placeholder, real data comes from APVTS masks at process time.
        {{0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}}
    }};

    // 6/8 patterns: 12 steps per bar, step = eighth-note. Beats accent on
    // step 0 and step 6 (the two dotted-quarter pulses).
    static constexpr std::array<std::array<juce::uint8, 12>, (size_t) PatternId::NumPatterns> kPatterns68
    {{
        // Off
        {{0,0,0, 0,0,0, 0,0,0, 0,0,0}},
        // Pulse: tom on the two main pulses
        {{MaskTom,0,0, 0,0,0, MaskTom,0,0, 0,0,0}},
        // Mist: gentle hat sway
        {{MaskHat,0,0, MaskHat,0,MaskHat, 0,0,MaskHat, 0,MaskHat,0}},
        // Stride: kick on 1, tom on 4
        {{MaskKick,0,0, 0,0,0, MaskTom,0,0, 0,0,0}},
        // Roam: layered kick / tom / hats (12-step variant of Roam)
        {{MaskKick|MaskHat,0,MaskHat, 0,MaskHat,0, MaskTom,0,MaskHat, 0,MaskHat,0}},
        // Custom placeholder
        {{0,0,0, 0,0,0, 0,0,0, 0,0,0}}
    }};

    // ─── Kick voice ───────────────────────────────────────────────────
    void triggerKick() { kickAge = 0; kickPhase = 0.0; }
    void renderKick (float& l, float& r) noexcept
    {
        if (kickAge < 0) return;
        const double t = kickAge / sampleRate;
        // Pitch sweep: 145 → 48 (50 ms) → 40 (next 500 ms)
        const float freq = (t < 0.05)
            ? 145.0f * std::pow (48.0f / 145.0f, (float) (t / 0.05))
            : 48.0f  * std::pow (40.0f / 48.0f,  (float) ((t - 0.05) / 0.50));

        kickPhase += (double) freq / sampleRate;
        if (kickPhase >= 1.0) kickPhase -= 1.0;
        const float sample = (float) std::sin (kickPhase * juce::MathConstants<double>::twoPi);

        // Envelope: 1 ms ramp-in, 80 ms punch decay, 470 ms tail.
        const float env = (t < 0.001) ? (float) (t / 0.001) * 1.2f
            : (t < 0.08) ? 1.2f * std::exp (-(float) (t - 0.001) * 12.0f)
            : (t < 0.55) ? 0.55f * std::exp (-(float) (t - 0.08) * 5.0f)
            : 0.0f;

        l += sample * env * 0.6f;
        r += sample * env * 0.6f;
        if (t > 0.6) kickAge = -1;
        else kickAge++;
    }

    // ─── Tom voice ────────────────────────────────────────────────────
    void triggerTom() { tomAge = 0; tomPhase = 0.0; }
    void renderTom (float& l, float& r) noexcept
    {
        if (tomAge < 0) return;
        const double t = tomAge / sampleRate;
        const float freq = (t < 0.05)
            ? 260.0f * std::pow (110.0f / 260.0f, (float) (t / 0.05))
            : 110.0f * std::pow (95.0f  / 110.0f, (float) ((t - 0.05) / 0.30));

        tomPhase += (double) freq / sampleRate;
        if (tomPhase >= 1.0) tomPhase -= 1.0;
        const float sample = (float) std::sin (tomPhase * juce::MathConstants<double>::twoPi);

        const float env = (t < 0.001) ? (float) (t / 0.001) * 0.85f
            : (t < 0.06) ? 0.85f * std::exp (-(float) (t - 0.001) * 12.0f)
            : (t < 0.40) ? 0.38f * std::exp (-(float) (t - 0.06) * 7.0f)
            : 0.0f;

        // Subtle left bias on tom, like the standalone (-0.12 pan).
        l += sample * env * 0.55f;
        r += sample * env * 0.45f;
        if (t > 0.45) tomAge = -1;
        else tomAge++;
    }

    // ─── Hat voice (filtered noise) ───────────────────────────────────
    void triggerHat() { hatAge = 0; }
    void renderHat (float& l, float& r) noexcept
    {
        if (hatAge < 0) return;
        const double t = hatAge / sampleRate;
        // White noise → 1-pole HP at ~1700 Hz → 1-pole LP at ~4200 Hz
        // (matches the standalone's "lofi shaker" character).
        const float noise = hatRng.nextFloat() * 2.0f - 1.0f;
        const float aHP = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                            * 1700.0f / (float) sampleRate);
        const float aLP = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                            * 4200.0f / (float) sampleRate);
        hatLp += aLP * (noise - hatLp);
        const float bandPassed = noise - hatLp; // crude HP via subtraction
        hatHp += aHP * (bandPassed - hatHp);
        const float sample = bandPassed - hatHp;

        const float env = (t < 0.001) ? (float) (t / 0.001) * 0.22f
            : (t < 0.06) ? 0.22f * std::exp (-(float) (t - 0.001) * 60.0f)
            : 0.0f;

        l += sample * env * 0.45f;
        r += sample * env * 0.55f;
        if (t > 0.08) hatAge = -1;
        else hatAge++;
    }

    double      sampleRate    = 44100.0;
    juce::int64 lastFiredStep = -1;

    int    kickAge = -1; double kickPhase = 0.0;
    int    tomAge  = -1; double tomPhase  = 0.0;
    int    hatAge  = -1;
    float  hatHp = 0.0f, hatLp = 0.0f;
    juce::Random hatRng;
};
