#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <array>

// Granular Texture layer — direct port of the standalone's grainStream()
// (public/index.html). Plucked dulcimer sample (embedded BinaryData) is
// chopped into Hann-windowed grains, pitched up/down to match the held
// MIDI notes, and sprinkled across multiple parallel streams. About 60%
// of grains play in reverse for that "bowed swell" character.
class Texture
{
public:
    void prepare (double sr, int /*blockSize*/)
    {
        sampleRate = sr;

        if (! sampleLoaded)
            loadEmbeddedSample();

        for (auto& g : grains) g.active = false;
        for (auto& sched : streamSchedule) sched = 0;
    }

    void reset()
    {
        for (auto& g : grains) g.active = false;
        for (auto& sched : streamSchedule) sched = 0;
    }

    // Adds the granular layer onto `buffer`. `vol` is the texture level
    // (0 = silent). `heldNotes` is the current chord pool — pitches grains.
    // `octUp` adds an extra octave on top of every stream.
    void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                  const std::vector<int>& heldNotes,
                  float vol, bool octUp)
    {
        if (vol < 1e-4f || ! sampleLoaded) return;

        auto* L = buffer.getWritePointer (0) + startSample;
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) + startSample : L;

        for (int s = 0; s < numSamples; ++s)
        {
            // Stream scheduler — each stream periodically spawns a new grain.
            // When no notes are held we still tick the stream timers down so
            // they fire promptly when a note arrives.
            for (int streamId = 0; streamId < kNumStreams; ++streamId)
            {
                if (streamSchedule[(size_t) streamId] > 0)
                {
                    streamSchedule[(size_t) streamId]--;
                    continue;
                }

                if (! heldNotes.empty())
                    spawnGrain (streamId, heldNotes, octUp);

                // Reschedule next grain — random within ~0.4–0.7 s @ 44.1k.
                streamSchedule[(size_t) streamId] =
                    (int) ((0.35 + rng.nextFloat() * 0.35) * sampleRate);
            }

            // Render every active grain.
            float l = 0.0f, r = 0.0f;
            for (auto& g : grains)
                if (g.active)
                    g.render (l, r);

            L[s] += l * vol * 0.8f;   // standalone uses vol*0.8 on the bus
            R[s] += r * vol * 0.8f;
        }
    }

private:
    static constexpr int kNumStreams  = 5;
    static constexpr int kMaxGrains   = 24;
    static constexpr float kC2Hz      = 65.4064f;  // dulcimer sample's natural pitch
    static constexpr int kMaxSampleLen = 1 << 19;  // ~12 s @ 44.1k, plenty for our 5 s sample

    struct Grain
    {
        bool   active = false;
        bool   reverse = false;
        const float* sampleData = nullptr;
        int    sampleLen = 0;
        double readPos = 0.0;     // fractional read into sampleData
        double readRate = 1.0;
        int    durSamples = 0;
        int    age = 0;
        float  level = 0.0f;
        float  panL = 0.7f, panR = 0.7f;

        void render (float& l, float& r) noexcept
        {
            if (age >= durSamples) { active = false; return; }

            // Hann window with 30% fade in/out at the edges. Matches the
            // standalone's linear-ramp envelope but sounds smoother.
            const float pos = (float) age / (float) durSamples;
            const float hann = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * pos));

            // Linear-interp read into the dulcimer sample buffer.
            int idx = reverse ? (sampleLen - 1 - (int) readPos) : (int) readPos;
            idx = juce::jlimit (0, sampleLen - 2, idx);
            const float frac = (float) (readPos - std::floor (readPos));
            const float s0 = sampleData[idx];
            const float s1 = sampleData[idx + 1];
            const float sample = s0 * (1.0f - frac) + s1 * frac;

            const float gained = sample * level * hann;
            l += gained * panL;
            r += gained * panR;

            readPos += readRate;
            ++age;
        }
    };

    void loadEmbeddedSample();

    void spawnGrain (int streamId,
                     const std::vector<int>& heldNotes,
                     bool octUp)
    {
        // Find a free grain slot.
        int slot = -1;
        for (int i = 0; i < kMaxGrains; ++i)
            if (! grains[(size_t) i].active) { slot = i; break; }
        if (slot < 0) return;
        auto& g = grains[(size_t) slot];

        // Each stream targets a different note in the chord; streams 3–4
        // double an octave up for shimmer (matches the standalone).
        const int   noteIdx  = streamId % (int) heldNotes.size();
        const int   midi     = heldNotes[(size_t) noteIdx];
        const int   octShift = ((streamId >= 3) ? 1 : 0) + (octUp ? 1 : 0);
        const float freq     = (float) (juce::MidiMessage::getMidiNoteInHertz (midi)
                                         * std::pow (2.0, (double) octShift));
        const float ratio    = freq / kC2Hz;

        // Slight pitch jitter so grains don't beat into a perfect comb.
        const float centsJitter = (rng.nextFloat() - 0.5f) * 24.0f;
        const float finalRate   = ratio * std::pow (2.0f, centsJitter / 1200.0f);

        // Grain duration: ~430–800 ms before pitch scaling.
        const float baseDur = (0.43f + rng.nextFloat() * 0.37f);
        const float grainSec = baseDur / juce::jmax (0.2f, finalRate);
        g.durSamples = juce::jmax (32, (int) (grainSec * sampleRate));

        // Random start position in the source — avoid the very first/last 10%
        // so we don't pick up the editing margins.
        const int margin = sampleLength / 10;
        const int playLen = (int) (baseDur * sourceSampleRate);
        const int maxStart = juce::jmax (margin + 1, sampleLength - playLen - margin);
        const int start = margin + (int) (rng.nextFloat() * (float) (maxStart - margin));

        g.sampleData = sampleData.data();
        g.sampleLen  = sampleLength;
        g.reverse    = rng.nextFloat() < 0.6f;        // ~60% reverse, like standalone
        g.readPos    = (double) start;
        g.readRate   = (double) finalRate * (sourceSampleRate / sampleRate);
        g.age        = 0;
        g.level      = (streamId >= 3) ? 0.12f : 0.15f;

        // Stream pan spread.
        const float basePan  = (float) (streamId - 2) * 0.3f;
        const float jitter   = (rng.nextFloat() - 0.5f) * 0.7f;
        const float pan      = juce::jlimit (-1.0f, 1.0f, basePan + jitter);
        const float panAng   = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        g.panL = std::cos (panAng);
        g.panR = std::sin (panAng);

        g.active = true;
    }

    double sampleRate = 44100.0;
    double sourceSampleRate = 22050.0;
    bool sampleLoaded = false;

    std::vector<float> sampleData;
    int sampleLength = 0;

    std::array<Grain, kMaxGrains> grains;
    std::array<int, kNumStreams>  streamSchedule { 0, 0, 0, 0, 0 };
    juce::Random rng;
};
