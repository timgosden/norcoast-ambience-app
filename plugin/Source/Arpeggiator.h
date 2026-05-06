#pragma once

#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Simple arpeggiator inspired by the standalone's `fireArpNote`. Watches
// the host's MidiKeyboardState (which reflects both external MIDI and the
// on-screen keyboard) and at each rhythmic subdivision plays one of the
// held notes through an internal triangle/saw voice with a short envelope.
//
// Octave-span: optionally extends the held-notes pool with copies +12 or
// +24 semitones above so the arp climbs over multiple octaves.
class Arpeggiator
{
public:
    enum class VoiceKind { Triangle = 0, Saw, Sine };

    void prepare (double sr, int /*blockSize*/)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        sampleCounter = 0;
        stepIdx       = 0;
        env           = 0.0f;
        envState      = 0;
        phase         = 0.0;
        phaseInc      = 0.0;
        currentNote   = -1;
    }

    // Render `numSamples` of arpeggiator audio added on top of `buffer`.
    //
    //   vol        : 0..1   (0 = off)
    //   rateBeats  : note duration in beats (0.25 = 16th, 0.5 = 8th, 1 = 1/4)
    //   bpm        : host tempo (or default 120)
    //   octSpan    : 0..2 — number of extra octaves above the held notes
    //   voice      : VoiceKind selector
    void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                  const std::vector<int>& heldNotes,
                  float vol, float rateBeats, double bpm,
                  int octSpan, VoiceKind voice)
    {
        if (vol < 1e-4f || heldNotes.empty())
        {
            envState = 0; env = 0.0f;
            return;
        }

        const int patternSize  = (int) heldNotes.size() * (octSpan + 1);
        const int tickSamples  = juce::jmax (1, (int) ((60.0 / bpm) * rateBeats * sampleRate));
        const int noteDur      = (int) (tickSamples * 0.88f);   // 12% overlap → legato
        const int attackSamps  = juce::jmin (tickSamples / 8, (int) (0.04 * sampleRate));
        const int releaseSamps = juce::jmin (tickSamples / 4, (int) (0.30 * sampleRate));

        auto* L = buffer.getWritePointer (0) + startSample;
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) + startSample : L;

        for (int s = 0; s < numSamples; ++s)
        {
            // Tick scheduler — drop a new note every `tickSamples`.
            if (sampleCounter <= 0)
            {
                triggerNextNote (heldNotes, octSpan);
                sampleCounter = tickSamples;
                envState  = 1;        // attacking
                envTimer  = 0;
                stepIdx   = (stepIdx + 1) % juce::jmax (1, patternSize);
            }
            --sampleCounter;

            // Envelope state machine.
            envTimer++;
            if (envState == 1) // attack
            {
                env += 1.0f / juce::jmax (1, attackSamps);
                if (env >= 1.0f) { env = 1.0f; envState = 2; envTimer = 0; }
            }
            else if (envState == 2 && envTimer > noteDur) // start release
            {
                envState = 3;
                envTimer = 0;
            }
            else if (envState == 3)
            {
                env -= 1.0f / juce::jmax (1, releaseSamps);
                if (env <= 0.0f) { env = 0.0f; envState = 0; }
            }

            // Oscillator
            float sample = 0.0f;
            if (envState != 0)
            {
                phase += phaseInc;
                if (phase >= 1.0) phase -= 1.0;

                switch (voice)
                {
                    case VoiceKind::Saw:
                        sample = (float) (phase * 2.0 - 1.0);
                        break;
                    case VoiceKind::Sine:
                        sample = (float) std::sin (phase * juce::MathConstants<double>::twoPi);
                        break;
                    case VoiceKind::Triangle:
                    default:
                    {
                        // 4·|p−0.5| − 1 — fast triangle without abs branches.
                        const double p = phase;
                        sample = (float) (4.0 * std::abs (p - 0.5) - 1.0);
                        break;
                    }
                }
            }

            const float gain = env * vol * 0.4f;   // master arp scaling
            L[s] += sample * gain;
            R[s] += sample * gain;
        }
    }

private:
    void triggerNextNote (const std::vector<int>& heldNotes, int octSpan)
    {
        const int patSize = (int) heldNotes.size() * (octSpan + 1);
        if (patSize == 0) return;

        const int idxInOctave = stepIdx % (int) heldNotes.size();
        const int octShift    = (stepIdx / (int) heldNotes.size()) % (octSpan + 1);
        currentNote = heldNotes[(size_t) idxInOctave] + octShift * 12;
        const double freq = juce::MidiMessage::getMidiNoteInHertz (currentNote);
        phaseInc = freq / sampleRate;
        phase    = 0.0;
    }

    double sampleRate    = 44100.0;
    int    sampleCounter = 0;
    int    stepIdx       = 0;
    int    currentNote   = -1;
    double phase         = 0.0;
    double phaseInc      = 0.0;

    float  env           = 0.0f;
    int    envState      = 0;     // 0=off, 1=attack, 2=sustain, 3=release
    int    envTimer      = 0;
};
