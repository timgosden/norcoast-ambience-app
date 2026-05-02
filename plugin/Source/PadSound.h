#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Marker sound — voices accept any incoming MIDI note via canPlaySound().
// Replaced in phase 2b with a per-layer sound carrying the timbre def.
struct PadSound : public juce::SynthesiserSound
{
    bool appliesToNote    (int)  override { return true; }
    bool appliesToChannel (int)  override { return true; }
};
