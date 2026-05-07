#pragma once

#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Waves.h"

// One pad layer's full DSP recipe — direct port of an entry in `PAD_LAYERS`
// from the standalone (public/index.html).
struct LayerConfig
{
    struct Timbre
    {
        WaveType wave;
        int   count;   // unison voices for this timbre
        float ds;      // detune-spread cents (×1.25 internal scale to match standalone)
        float gain;    // per-osc gain
        float pan;     // per-osc pan amount (×0.65 internal scale)
    };

    juce::String name;
    std::vector<Timbre> timbres;
    std::vector<int>    octaves   { 0 };  // octave offsets relative to the played note

    float fBase     = 200.0f;             // pre-1.5× cutoff (matches standalone fBase)
    float fMod      = 30.0f;              // filter LFO depth (Hz)
    float fRate     = 0.1f;               // filter LFO rate (Hz)
    float aMod      = 0.03f;              // amp LFO depth
    float aRate     = 0.12f;              // amp LFO rate (Hz)
    float layerGain = 1.0f;               // matches DEFAULT_VOLUMES in standalone

    juce::ADSR::Parameters adsr { 7.0f, 0.5f, 1.0f, 7.0f };

    int totalOscCount() const noexcept
    {
        int sum = 0;
        for (auto& t : timbres) sum += t.count;
        return sum * (int) octaves.size();
    }
};
