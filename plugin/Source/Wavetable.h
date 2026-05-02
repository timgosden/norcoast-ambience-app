#pragma once

#include <array>
#include <vector>
#include <cmath>

// Single-cycle additive-synthesis wavetable, mirroring the standalone's
// `makeWave(harmonics)` helper. Harmonic amplitudes are passed in
// fundamental-first order (h[0] is the fundamental).
class Wavetable
{
public:
    static constexpr int kSize = 2048;

    void build (const std::vector<float>& harmonics)
    {
        constexpr double tau = 6.283185307179586476925286766559;
        float peak = 0.0f;

        for (int i = 0; i < kSize; ++i)
        {
            const double phase = (double) i / kSize * tau;
            float sum = 0.0f;
            for (size_t h = 0; h < harmonics.size(); ++h)
                sum += harmonics[h] * (float) std::sin (phase * (double) (h + 1));
            table[i] = sum;
            peak = std::max (peak, std::abs (sum));
        }

        // Normalise to ±1 — same effect as Web Audio's createPeriodicWave
        // with disableNormalization:false.
        if (peak > 0.0f)
        {
            const float inv = 1.0f / peak;
            for (auto& s : table) s *= inv;
        }
    }

    // Linear interpolation between adjacent samples. phase01 is in [0, 1).
    float sample (double phase01) const noexcept
    {
        const double idx = phase01 * (double) kSize;
        const int    i0  = (int) idx;
        const int    i1  = (i0 + 1) % kSize;
        const float  f   = (float) (idx - (double) i0);
        return table[(size_t) i0] * (1.0f - f) + table[(size_t) i1] * f;
    }

private:
    std::array<float, kSize> table {};
};
