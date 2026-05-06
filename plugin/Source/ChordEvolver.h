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
    enum ChordType { Fifth, Sus2, Sus4, Major, Minor, Maj7, Ninth, NumTypes };

    static juce::StringArray getChordNames()
    {
        return { "5th", "Sus2", "Sus4", "Major", "Minor", "Maj7", "9th" };
    }

    static const std::vector<int>& intervalsFor (int type)
    {
        static const std::array<std::vector<int>, (size_t) NumTypes> data
        {{
            { 7 },          // 5th        — open root + fifth
            { 2, 7 },       // Sus2
            { 5, 7 },       // Sus4
            { 4, 7 },       // Major
            { 3, 7 },       // Minor
            { 7, 11 },      // Maj7 (sparse — no 3rd, like the standalone)
            { 7, 14 }       // 9th
        }};
        return data[(size_t) juce::jlimit (0, (int) NumTypes - 1, type)];
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
                  int targetType, bool evolveOn, float rateSeconds,
                  AdvanceFn&& advanceTargetParam)
    {
        // ─── Auto-evolve: advance the target chord type periodically ──
        if (evolveOn)
        {
            sampleCounter += numSamples;
            const double samplesPerCycle = (double) juce::jmax (0.5f, rateSeconds) * sampleRate;
            if (sampleCounter >= samplesPerCycle)
            {
                sampleCounter -= samplesPerCycle;
                const int next = (targetType + 1) % (int) NumTypes;
                advanceTargetParam (next);
                targetType = next;
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
        if (targetType != activeChordType)
        {
            const auto& oldIv = intervalsFor (activeChordType);
            const auto& newIv = intervalsFor (targetType);
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
            activeChordType = targetType;
        }

        // ─── User events + augmentation ───────────────────────────────
        const auto& iv = intervalsFor (activeChordType);
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
    int    activeChordType = 0;
    std::array<bool, 128> activeRoots {};
};
