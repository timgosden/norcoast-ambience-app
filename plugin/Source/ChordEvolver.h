#pragma once

#include <array>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// The standalone web app's defining feature: a single root note becomes
// a chord, and the chord shape evolves over time. In the plugin we
// receive MIDI from the player (or DAW) and augment each held note with
// extra notes at the current chord-type intervals. When auto-evolve is
// on, the chord type cycles through the enabled set every N seconds.
//
// All intervals are relative to the held root and follow the standalone's
// CHORD_TYPES table — sparse, ambient voicings (no third on the 5th /
// Sus / 9th chords) plus the obvious Major/Minor triads for variety.
class ChordEvolver
{
public:
    // Chord types match the standalone web app's CHORD_TYPES exactly.
    // No major/minor triads — the standalone is intentionally about
    // open / sus / sparse voicings for ambient pads. Custom is a
    // bitmask over the 7 major-scale degrees (1=root is implicit).
    enum ChordType { Fifth, Sus2, Sus4, Maj7, Ninth, Custom, NumTypes };

    static juce::StringArray getChordNames()
    {
        return { "5th", "Sus2", "Sus4", "Maj7th", "9th", "Custom" };
    }

    // Major-scale degree → semitones from root: 1, 2, 3, 4, 5, 6, 7
    static constexpr std::array<int, 7> kMajorScaleDegrees { 0, 2, 4, 5, 7, 9, 11 };

    // For Custom chord, the bitmask must be passed in (a static
    // intervalsFor() can't see it). Mask bit i (1..6) = include
    // major-scale degree (i+1). Bit 0 = root, always implicit.
    std::vector<int> intervalsForCurrent (int type, int customMask) const
    {
        if (type == Custom)
        {
            std::vector<int> ivs;
            for (int i = 1; i < 7; ++i)
                if (customMask & (1 << i))
                    ivs.push_back (kMajorScaleDegrees[(size_t) i]);
            if (ivs.empty()) ivs.push_back (7);   // safety: at least the 5th
            return ivs;
        }
        return intervalsFor (type);
    }

    static const std::vector<int>& intervalsFor (int type)
    {
        static const std::vector<int> empty;
        static const std::array<std::vector<int>, 5> data
        {{
            { 7 },          // 5th
            { 2, 7 },       // Sus2
            { 5, 7 },       // Sus4
            { 11 },         // Maj7th — sparse (root + maj7 only)
            { 7, 14 }       // 9th
        }};
        if (type < 0 || type >= (int) data.size()) return empty;
        return data[(size_t) type];
    }

    void prepare (double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        sampleCounter   = 0;
        activeChordType = 0;
        for (auto& a : activeRoots) a = false;
    }

    int getCurrentChordType() const noexcept { return activeChordType; }

    // Process a MIDI buffer in place — augment user note-ons/offs with
    // chord-interval events, and run the auto-evolve timer.
    //
    //   targetType         : current value of the chord-type param
    //   evolveOn           : auto-cycle chord type
    //   rateSeconds        : seconds between auto-evolve cycles
    //   numSamples         : block size
    //   channel            : MIDI channel for emitted events (1..16)
    //   advanceTargetParam : callback invoked when auto-evolve picks a
    //                        new chord-type value, so the host param
    //                        can be updated to reflect the change
    template <typename AdvanceFn>
    void process (juce::MidiBuffer& midi, int numSamples, int channel,
                  int targetType, int customMask, int enabledMask,
                  bool evolveOn, float rateBeats, double bpm,
                  AdvanceFn&& advanceTargetParam)
    {
        // ─── Auto-evolve: advance the target chord type periodically.
        // Tempo-locked: cycle every rateBeats beats at the host BPM, so
        // chord changes line up with the arp + drum tempo grid.
        if (evolveOn)
        {
            sampleCounter += numSamples;
            const double samplesPerCycle = (60.0 / juce::jmax (1.0, bpm))
                                            * (double) juce::jmax (1.0f, rateBeats)
                                            * sampleRate;
            if (sampleCounter >= samplesPerCycle)
            {
                sampleCounter -= samplesPerCycle;

                // Pick the next chord that's enabled in the pool. If
                // none are enabled, just stay on the current one.
                int next = targetType;
                if (enabledMask & ((1 << (int) NumTypes) - 1))
                {
                    for (int step = 1; step <= (int) NumTypes; ++step)
                    {
                        const int candidate = (targetType + step) % (int) NumTypes;
                        if (enabledMask & (1 << candidate))
                        {
                            next = candidate;
                            break;
                        }
                    }
                }
                if (next != targetType)
                {
                    advanceTargetParam (next);
                    targetType = next;
                }
            }
        }
        else
        {
            sampleCounter = 0;
        }

        targetType = juce::jlimit (0, (int) NumTypes - 1, targetType);

        // Move all incoming events into a scratch buffer so we can
        // re-emit them alongside the chord intervals.
        juce::MidiBuffer userEvents;
        userEvents.swapWith (midi);

        // ─── Chord-type transition ────────────────────────────────────
        // Off the old intervals on every active root, then on the new.
        if (targetType != activeChordType || customMask != activeCustomMask)
        {
            const auto oldIv = intervalsForCurrent (activeChordType, activeCustomMask);
            const auto newIv = intervalsForCurrent (targetType,      customMask);
            for (int root = 0; root < 128; ++root)
            {
                if (! activeRoots[(size_t) root]) continue;
                for (int iv : oldIv)
                {
                    const int n = root + iv;
                    if (n >= 0 && n < 128)
                        midi.addEvent (juce::MidiMessage::noteOff (channel, n), 0);
                }
                for (int iv : newIv)
                {
                    const int n = root + iv;
                    if (n >= 0 && n < 128)
                        midi.addEvent (juce::MidiMessage::noteOn (channel, n, 0.7f), 0);
                }
            }
            activeChordType   = targetType;
            activeCustomMask  = customMask;
        }

        // ─── User events + augmentation ───────────────────────────────
        const auto iv = intervalsForCurrent (activeChordType, activeCustomMask);
        for (const auto meta : userEvents)
        {
            const auto msg = meta.getMessage();
            const int sample = meta.samplePosition;

            const bool isOn  = msg.isNoteOn()  && msg.getVelocity() > 0;
            const bool isOff = msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0);

            if (isOn)
            {
                const int root = msg.getNoteNumber();
                const float vel = msg.getFloatVelocity();
                midi.addEvent (msg, sample);
                if (root >= 0 && root < 128) activeRoots[(size_t) root] = true;
                for (int interval : iv)
                {
                    const int n = root + interval;
                    if (n >= 0 && n < 128)
                        midi.addEvent (juce::MidiMessage::noteOn (channel, n, vel), sample);
                }
            }
            else if (isOff)
            {
                const int root = msg.getNoteNumber();
                midi.addEvent (msg, sample);
                if (root >= 0 && root < 128) activeRoots[(size_t) root] = false;
                for (int interval : iv)
                {
                    const int n = root + interval;
                    if (n >= 0 && n < 128)
                        midi.addEvent (juce::MidiMessage::noteOff (channel, n), sample);
                }
            }
            else
            {
                midi.addEvent (msg, sample);
            }
        }
    }

private:
    double sampleRate     = 44100.0;
    double sampleCounter  = 0.0;
    int    activeChordType  = 0;
    int    activeCustomMask = 0;
    std::array<bool, 128> activeRoots {};
};
