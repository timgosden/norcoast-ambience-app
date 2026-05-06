#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Arpeggiator that fires the standalone web app's three voice flavours
// (Soft / Sine / Juno) verbatim from public/index.html's fireArpNote.
//
//   Soft — triangle, 3 voices at pan -0.4 / 0 / +0.4 with -4 / 0 / +4 cent
//          detune, fixed lowpass at min(900 + freq*0.25, 2000) Hz, 40 ms
//          attack, peak gain = arpVol * 0.20.
//   Sine — sine, 2 voices at pan -0.3 / +0.3, ±3 cent detune, no filter,
//          40 ms attack, peak = arpVol * 0.23.
//   Juno — sawtooth, 3 voices at pan -0.75 / 0 / +0.75, ±13 cent detune,
//          22 ms attack, lowpass exp-sweep from bright (min(2400+freq*1.1,
//          6500) Hz, Q 0.9) to dark (max(280+freq*0.13, 100) Hz) over the
//          decay phase. peak = arpVol * 0.165. This is the "pluck".
//
// Each tick allocates 2-3 voices from a 16-slot pool. Voices fade out
// over the back half of their note duration so successive arp notes
// crossfade into each other (the standalone overlaps by 12%).
class Arpeggiator
{
public:
    enum class VoiceKind { Soft = 0, Sine, Juno };

    void prepare (double sr, int /*blockSize*/)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v.active = false;
        sampleCounter = 0;
        stepIdx = 0;
    }

    void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                  const std::vector<int>& heldNotes,
                  float vol, float rateBeats, double bpm,
                  int octSpan, VoiceKind voiceKind)
    {
        const bool noteSourceLive = vol >= 1e-4f && ! heldNotes.empty();
        if (! noteSourceLive)
        {
            // Stop scheduling — but let any in-flight voices ring out.
            sampleCounter = 0;
            stepIdx = 0;
        }

        const int patternSize  = noteSourceLive
            ? (int) heldNotes.size() * (octSpan + 1) : 1;
        const int tickSamples  = juce::jmax (1, (int) ((60.0 / bpm) * rateBeats * sampleRate));
        const int noteDurSamps = (int) (tickSamples * 0.88f);

        auto* L = buffer.getWritePointer (0) + startSample;
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) + startSample : L;

        for (int s = 0; s < numSamples; ++s)
        {
            if (noteSourceLive && sampleCounter <= 0)
            {
                triggerArpNote (heldNotes, octSpan, voiceKind, vol, noteDurSamps);
                sampleCounter = tickSamples;
                stepIdx = (stepIdx + 1) % juce::jmax (1, patternSize);
            }
            if (sampleCounter > 0) --sampleCounter;

            float l = 0.0f, r = 0.0f;
            for (auto& v : voices)
                if (v.active)
                    renderVoice (v, l, r);

            L[s] += l;
            R[s] += r;
        }
    }

private:
    struct Voice
    {
        bool   active   = false;
        VoiceKind kind  = VoiceKind::Soft;
        // Oscillator
        double phase    = 0.0;
        double phaseInc = 0.0;
        // Pan
        float  panL     = 0.7f;
        float  panR     = 0.7f;
        // Envelope (4 break-points over a fixed number of samples)
        float  peak     = 0.0f;
        float  decayLevel = 0.0f;
        int    age      = 0;
        int    attackEnd  = 0;
        int    decayEnd   = 0;
        int    releaseStart = 0;   // sustain end / release start
        int    releaseEnd   = 0;
        // Filter (one-pole LP, optional)
        bool   hasFilter = false;
        float  filterZ   = 0.0f;
        float  filterCutoffNow   = 22000.0f;
        float  filterCutoffMul   = 1.0f;   // per-sample multiplier (>1 brightens, <1 darkens)
        float  filterCutoffFloor = 100.0f;
        float  filterCutoffCeil  = 22000.0f;
    };

    Voice* allocVoice() noexcept
    {
        for (auto& v : voices)
            if (! v.active) return &v;
        return nullptr;          // pool exhausted — drop this voice silently
    }

    void initVoice (Voice& v, VoiceKind kind,
                    float freq, float detuneCents,
                    float pan,
                    float peakLevel,
                    float decayFactor,
                    float attackSec,
                    float decaySec,
                    int   noteDurSamps) noexcept
    {
        v.active = true;
        v.kind   = kind;
        v.phase  = 0.0;
        v.phaseInc = (double) (freq * std::pow (2.0f, detuneCents / 1200.0f)) / sampleRate;

        const float panAng = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        v.panL = std::cos (panAng);
        v.panR = std::sin (panAng);

        v.peak       = peakLevel;
        v.decayLevel = peakLevel * decayFactor;
        v.age        = 0;
        v.attackEnd  = (int) (attackSec * sampleRate);
        v.decayEnd   = v.attackEnd + (int) (decaySec * sampleRate);
        v.releaseStart = juce::jmax (v.decayEnd, noteDurSamps - (int) (0.020 * sampleRate));
        v.releaseEnd   = noteDurSamps + (int) (0.30 * sampleRate);  // tail

        v.hasFilter = false;
        v.filterZ = 0.0f;
    }

    void setExpFilterSweep (Voice& v, float startHz, float endHz, int rampSamps) noexcept
    {
        v.hasFilter = true;
        v.filterCutoffNow   = startHz;
        v.filterCutoffFloor = juce::jmin (startHz, endHz);
        v.filterCutoffCeil  = juce::jmax (startHz, endHz);
        // Per-sample multiplier so cutoff = start * (end/start)^(age/rampSamps).
        v.filterCutoffMul = std::pow (endHz / juce::jmax (1.0f, startHz),
                                       1.0f / juce::jmax (1, rampSamps));
        v.filterZ = 0.0f;
    }

    void setStaticLowpass (Voice& v, float hz) noexcept
    {
        v.hasFilter = true;
        v.filterCutoffNow   = hz;
        v.filterCutoffFloor = hz;
        v.filterCutoffCeil  = hz;
        v.filterCutoffMul   = 1.0f;
        v.filterZ = 0.0f;
    }

    // ─── Triggers ─────────────────────────────────────────────────────
    void triggerArpNote (const std::vector<int>& heldNotes, int octSpan,
                         VoiceKind kind, float vol, int noteDurSamps)
    {
        const int idx = stepIdx % (int) heldNotes.size();
        const int oct = (stepIdx / (int) heldNotes.size()) % (octSpan + 1);
        const int midi = heldNotes[(size_t) idx] + oct * 12;
        const float freq = (float) juce::MidiMessage::getMidiNoteInHertz (midi);

        switch (kind)
        {
            case VoiceKind::Soft: triggerSoft (freq, vol, noteDurSamps); break;
            case VoiceKind::Sine: triggerSine (freq, vol, noteDurSamps); break;
            case VoiceKind::Juno: triggerJuno (freq, vol, noteDurSamps); break;
        }
    }

    void triggerSoft (float freq, float vol, int noteDurSamps)
    {
        struct Slot { float pan; float det; float vel; };
        const std::array<Slot, 3> slots
        {{
            { -0.4f, -4.0f, 0.9f  },
            {  0.0f,  0.0f, 0.75f },
            {  0.4f,  4.0f, 0.9f  }
        }};

        const float lpHz = juce::jmin (900.0f + freq * 0.25f, 2000.0f);
        const float decaySec = juce::jmin (noteDurSamps / (float) sampleRate * 0.4f, 0.6f);

        for (auto& s : slots)
        {
            if (Voice* v = allocVoice())
            {
                initVoice (*v, VoiceKind::Soft, freq, s.det, s.pan,
                           vol * 0.20f * s.vel, 0.65f, 0.04f, decaySec, noteDurSamps);
                setStaticLowpass (*v, lpHz);
            }
        }
    }

    void triggerSine (float freq, float vol, int noteDurSamps)
    {
        struct Slot { float pan; float det; };
        const std::array<Slot, 2> slots {{ { -0.3f, -3.0f }, { 0.3f, 3.0f } }};
        const float decaySec = juce::jmin (noteDurSamps / (float) sampleRate * 0.5f, 0.8f);

        for (auto& s : slots)
        {
            if (Voice* v = allocVoice())
                initVoice (*v, VoiceKind::Sine, freq, s.det, s.pan,
                           vol * 0.23f, 0.55f, 0.04f, decaySec, noteDurSamps);
        }
    }

    void triggerJuno (float freq, float vol, int noteDurSamps)
    {
        struct Slot { float pan; float det; float vel; };
        const std::array<Slot, 3> slots
        {{
            { -0.75f, -13.0f, 1.0f  },
            {  0.0f,    0.0f, 0.82f },
            {  0.75f,  13.0f, 1.0f  }
        }};

        const float bright   = juce::jmin (2400.0f + freq * 1.1f, 6500.0f);
        const float dark     = juce::jmax ( 280.0f + freq * 0.13f,  100.0f);
        const float decaySec = juce::jmin (noteDurSamps / (float) sampleRate * 0.35f, 0.9f);
        const int   rampSamps = juce::jmax (32, (int) (decaySec * sampleRate));

        for (auto& s : slots)
        {
            if (Voice* v = allocVoice())
            {
                initVoice (*v, VoiceKind::Juno, freq, s.det, s.pan,
                           vol * 0.165f * s.vel, 0.45f, 0.022f, decaySec, noteDurSamps);
                setExpFilterSweep (*v, bright, dark, rampSamps);
            }
        }
    }

    // ─── Per-sample render ────────────────────────────────────────────
    void renderVoice (Voice& v, float& outL, float& outR) noexcept
    {
        // Envelope: piece-wise linear over (attack, decay, sustain, release).
        float env;
        if      (v.age < v.attackEnd)
            env = (float) v.age / (float) juce::jmax (1, v.attackEnd) * v.peak;
        else if (v.age < v.decayEnd)
            env = juce::jmap ((float) (v.age - v.attackEnd),
                              0.0f, (float) juce::jmax (1, v.decayEnd - v.attackEnd),
                              v.peak, v.decayLevel);
        else if (v.age < v.releaseStart)
            env = v.decayLevel;
        else if (v.age < v.releaseEnd)
            env = juce::jmap ((float) (v.age - v.releaseStart),
                              0.0f, (float) juce::jmax (1, v.releaseEnd - v.releaseStart),
                              v.decayLevel, 0.0f);
        else
        {
            v.active = false;
            return;
        }

        // Oscillator
        v.phase += v.phaseInc;
        if (v.phase >= 1.0) v.phase -= 1.0;
        const float p = (float) v.phase;

        float sample = 0.0f;
        switch (v.kind)
        {
            case VoiceKind::Soft: // triangle: 4|p-0.5| - 1
                sample = 4.0f * std::abs (p - 0.5f) - 1.0f;
                break;
            case VoiceKind::Sine:
                sample = std::sin (p * juce::MathConstants<float>::twoPi);
                break;
            case VoiceKind::Juno: // sawtooth
                sample = 2.0f * p - 1.0f;
                break;
        }

        // Filter (one-pole LP) — optional, with optional exp sweep.
        if (v.hasFilter)
        {
            v.filterCutoffNow *= v.filterCutoffMul;
            v.filterCutoffNow = juce::jlimit (v.filterCutoffFloor, v.filterCutoffCeil,
                                              v.filterCutoffNow);
            const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                              * v.filterCutoffNow / (float) sampleRate);
            v.filterZ += a * (sample - v.filterZ);
            sample = v.filterZ;
        }

        const float gained = sample * env;
        outL += gained * v.panL;
        outR += gained * v.panR;
        v.age++;
    }

    double sampleRate    = 44100.0;
    int    sampleCounter = 0;
    int    stepIdx       = 0;
    std::array<Voice, 16> voices;
};
