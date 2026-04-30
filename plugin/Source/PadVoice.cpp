#include "PadVoice.h"

Wavetable PadVoice::subTable;
Wavetable PadVoice::warmTable;

void PadVoice::initWavetables()
{
    // Harmonic recipes copied verbatim from the standalone's initWaves().
    subTable.build  ({ 1.0f, 0.06f, 0.02f });
    warmTable.build ({ 1.0f, 0.5f, 0.33f, 0.15f, 0.08f, 0.04f, 0.02f, 0.01f });
}

// Foundation timbre def, ported from PAD_LAYERS in the standalone:
//   timbres: [
//     { wave: 'sub',  count: 3, ds: 1.2, gain: 0.14, pan: 0.05 },
//     { wave: 'warm', count: 2, ds: 2,   gain: 0.04, pan: 0.08 } ]
//   fBase: 200, octaves: [-1], rootOnly: true
//
// The voice spread / pan formula in the standalone (with spread = 0.5):
//   t           = count > 1 ? (i / (count - 1)) * 2 - 1 : 0     // -1..+1
//   detuneCents = t * ds  * spread * 2.5      = t * ds * 1.25
//   pan         = t * pan * (0.3 + spread * 0.7) = t * pan * 0.65   (rootOnly so no freq alternation)
namespace
{
    constexpr float kSpreadConst   = 1.25f;   // spread * 2.5 with spread=0.5
    constexpr float kPanConst      = 0.65f;   // 0.3 + 0.5 * 0.7
    constexpr float kFilterBaseHz  = 300.0f;  // fBase * 1.5

    inline void setPan (PadVoice::Osc& o, float pan)
    {
        // Constant-power equal-loudness pan. pan in [-1, +1].
        const float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        o.panL = std::cos (angle);
        o.panR = std::sin (angle);
    }

    inline double detunedHz (double baseHz, float cents)
    {
        return baseHz * std::pow (2.0, (double) cents / 1200.0);
    }
}

void PadVoice::startNote (int midiNoteNumber, float velocity,
                          juce::SynthesiserSound*, int)
{
    const auto sr = getSampleRate();

    // Foundation's octaves[-1]: play one octave below the played note.
    const double rootHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    const double baseHz = rootHz * 0.5;

    auto& rng = juce::Random::getSystemRandom();
    int   idx = 0;

    // 3 "sub" voices — count=3, ds=1.2, gain=0.14, pan=0.05
    for (int i = 0; i < 3; ++i)
    {
        const float t = ((float) i / 2.0f) * 2.0f - 1.0f;          // -1, 0, +1
        const float cents = t * 1.2f * kSpreadConst;
        const float pan   = t * 0.05f * kPanConst;

        oscs[idx].table    = &subTable;
        oscs[idx].gain     = 0.14f;
        oscs[idx].phase    = rng.nextDouble();
        oscs[idx].phaseInc = detunedHz (baseHz, cents) / sr;
        setPan (oscs[idx], pan);
        ++idx;
    }

    // 2 "warm" voices — count=2, ds=2, gain=0.04, pan=0.08
    for (int i = 0; i < 2; ++i)
    {
        const float t = (i == 0 ? -1.0f : 1.0f);
        const float cents = t * 2.0f * kSpreadConst;
        const float pan   = t * 0.08f * kPanConst;

        oscs[idx].table    = &warmTable;
        oscs[idx].gain     = 0.04f;
        oscs[idx].phase    = rng.nextDouble();
        oscs[idx].phaseInc = detunedHz (baseHz, cents) / sr;
        setPan (oscs[idx], pan);
        ++idx;
    }

    filterL.setCutoff (sr, kFilterBaseHz);
    filterR.setCutoff (sr, kFilterBaseHz);
    filterL.reset();
    filterR.reset();

    adsr.setSampleRate (sr);
    adsr.setParameters (adsrParams);
    adsr.noteOn();

    juce::ignoreUnused (velocity);
}

void PadVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        adsr.reset();
        clearCurrentNote();
    }
}

void PadVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                int startSample, int numSamples)
{
    if (! adsr.isActive())
        return;

    auto* const left  = outputBuffer.getWritePointer (0) + startSample;
    auto* const right = outputBuffer.getNumChannels() > 1
                            ? outputBuffer.getWritePointer (1) + startSample
                            : left;

    for (int s = 0; s < numSamples; ++s)
    {
        const float env = adsr.getNextSample();

        float sumL = 0.0f, sumR = 0.0f;
        for (auto& o : oscs)
        {
            const float sample = o.table->sample (o.phase) * o.gain;
            sumL += sample * o.panL;
            sumR += sample * o.panR;

            o.phase += o.phaseInc;
            if (o.phase >= 1.0)
                o.phase -= 1.0;
        }

        sumL = filterL.process (sumL);
        sumR = filterR.process (sumR);

        left [s] += sumL * env;
        right[s] += sumR * env;
    }

    if (! adsr.isActive())
        clearCurrentNote();
}
