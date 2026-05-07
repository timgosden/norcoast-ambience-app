#include "Waves.h"

namespace
{
    Wavetable sub, warm, lush, choir, celestial, glass, strings;
    bool initialised = false;
}

void Waves::init()
{
    if (initialised) return;

    // Harmonic amplitudes copied verbatim from the standalone's initWaves() so
    // the spectrum lines up sample-for-sample.
    sub.build       ({ 1.0f, 0.06f, 0.02f });
    warm.build      ({ 1.0f, 0.5f, 0.33f, 0.15f, 0.08f, 0.04f, 0.02f, 0.01f });
    lush.build      ({ 1.0f, 0.7f, 0.5f, 0.4f, 0.32f, 0.25f, 0.2f, 0.16f,
                       0.13f, 0.1f, 0.08f, 0.06f, 0.05f, 0.04f, 0.03f, 0.025f, 0.02f });
    choir.build     ({ 1.0f, 0.6f, 0.15f, 0.35f, 0.5f, 0.12f, 0.08f, 0.2f,
                       0.35f, 0.06f, 0.04f, 0.12f, 0.18f, 0.03f });
    celestial.build ({ 0.6f, 0.05f, 0.85f, 0.1f, 0.95f, 0.08f, 0.7f, 0.05f,
                       0.5f, 0.03f, 0.35f, 0.02f, 0.2f, 0.01f, 0.12f, 0.005f, 0.06f });
    glass.build     ({ 0.15f, 0.08f, 0.3f, 0.12f, 0.5f, 0.15f, 0.45f, 0.2f,
                       0.55f, 0.25f, 0.45f, 0.2f, 0.35f, 0.15f, 0.25f, 0.1f, 0.18f });
    strings.build   ({ 1.0f, 0.45f, 0.8f, 0.25f, 0.55f, 0.18f, 0.38f, 0.12f,
                       0.25f, 0.08f, 0.15f, 0.05f, 0.08f, 0.03f });
    initialised = true;
}

const Wavetable& Waves::get (WaveType w)
{
    switch (w)
    {
        case WaveType::Sub:       return sub;
        case WaveType::Warm:      return warm;
        case WaveType::Lush:      return lush;
        case WaveType::Choir:     return choir;
        case WaveType::Celestial: return celestial;
        case WaveType::Glass:     return glass;
        case WaveType::Strings:   return strings;
    }
    return sub;
}
