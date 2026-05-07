#pragma once

#include "Wavetable.h"

// Single-cycle wavetables used across all pad layers. Mirrors the standalone's
// `waves` map (`initWaves` in public/index.html) — exact same harmonic series
// so the plugin and the web app are sonically interchangeable.
enum class WaveType { Sub, Warm, Lush, Choir, Celestial, Glass, Strings };

struct Waves
{
    static void init();                          // call once on plugin construction
    static const Wavetable& get (WaveType w);    // realtime-safe lookup
};
