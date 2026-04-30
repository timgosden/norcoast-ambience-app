#include "PadVoice.h"

Wavetable PadVoice::subTable;
Wavetable PadVoice::warmTable;

void PadVoice::initWavetables()
{
    subTable.build  ({ 1.0f, 0.06f, 0.02f });
    warmTable.build ({ 1.0f, 0.5f, 0.33f, 0.15f, 0.08f, 0.04f, 0.02f, 0.01f });
}

// Foundation timbre def, ported from PAD_LAYERS in the standalone:
//   timbres: [
//     { wave: 'sub',  count: 3, ds: 1.2, gain: 0.14, pan: 0.05 },
//     { wave: 'warm', count: 2, ds: 2,   gain: 0.04, pan: 0.08 } ]
//   fBase: 200, fMod: 30, fRate: 0.1, aMod: 0.03, aRate: 0.12
//   octaves: [-1], rootOnly: true
namespace
{
    constexpr float kSpreadConst   = 1.25f;   // spread * 2.5 with spread=0.5
    constexpr float kPanConst      = 0.65f;   // 0.3 + 0.5 * 0.7
    constexpr float kFilterBaseHz  = 300.0f;  // fBase * 1.5

    constexpr float kFRate = 0.1f;
    constexpr float kFMod  = 30.0f;
    constexpr float kARate = 0.12f;
    constexpr float kAMod  = 0.03f;

    inline void setPan (PadVoice::Osc& o, float pan)
    {
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
    auto& rng = juce::Random::getSystemRandom();

    // Foundation's octaves[-1] — play one octave below the played note.
    const double rootHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    const double baseHz = rootHz * 0.5;

    auto initOsc = [&] (Osc& o, const Wavetable& tab, float t,
                        float ds, float gain, float panAmt)
    {
        const float cents = t * ds * kSpreadConst;
        const float pan   = t * panAmt * kPanConst;

        o.table       = &tab;
        o.gain        = gain;
        o.phase       = rng.nextDouble();
        o.baseHz      = baseHz;
        o.staticCents = cents;
        o.phaseInc    = detunedHz (baseHz, cents) / sr;
        setPan (o, pan);

        // Per-voice detune LFO: rate 0.01 - 0.06 Hz, depth ~0.1 - 0.94 cents.
        // Matches the standalone's per-osc detune LFO formula:
        //   rate  = 0.01 + random * 0.05
        //   depth = (0.2 + random * 1.5) * (0.1 + spread * 0.9)   with spread = 0.5
        const double lfoRate  = 0.01 + rng.nextDouble() * 0.05;
        const float  lfoDepth = (0.2f + (float) rng.nextDouble() * 1.5f) * 0.55f;
        o.detuneLFO.setup (sr, lfoRate, lfoDepth, rng.nextDouble());
    };

    int idx = 0;
    // 3 "sub" voices
    for (int i = 0; i < 3; ++i)
    {
        const float t = ((float) i / 2.0f) * 2.0f - 1.0f;     // -1, 0, +1
        initOsc (oscs[idx++], subTable, t, /*ds*/ 1.2f, /*gain*/ 0.14f, /*pan*/ 0.05f);
    }
    // 2 "warm" voices
    for (int i = 0; i < 2; ++i)
    {
        const float t = (i == 0 ? -1.0f : 1.0f);
        initOsc (oscs[idx++], warmTable, t, /*ds*/ 2.0f, /*gain*/ 0.04f, /*pan*/ 0.08f);
    }

    // Master LFOs.
    // filterLFO1: fRate Hz, depth fMod (Hz). Random start phase.
    filterLFO1.setup (sr, kFRate, kFMod, rng.nextDouble());

    // filterLFO2: incommensurate ~30% of fRate, 45% depth, random phase
    // including a 0-4 s start delay (we collapse that to a phase offset).
    const double lfo2Rate = (double) kFRate * (0.27 + rng.nextDouble() * 0.13);
    filterLFO2.setup (sr, lfo2Rate, kFMod * 0.45f, rng.nextDouble());

    // ampLFO: aRate Hz, depth aMod (fraction of unity gain).
    ampLFO.setup (sr, kARate, kAMod, rng.nextDouble());

    // breathLFO: ~80 s period, depth 0.03.
    const double breathRate = 0.011 + rng.nextDouble() * 0.006;
    breathLFO.setup (sr, breathRate, 0.03f, rng.nextDouble());

    filterBaseHz = kFilterBaseHz;
    filterL.setCutoff (sr, filterBaseHz);
    filterR.setCutoff (sr, filterBaseHz);
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
        adsr.noteOff();
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

    const auto sr = getSampleRate();

    // ── Block-rate modulation ──────────────────────────────────────────
    const float fMod1     = filterLFO1.advance (numSamples);
    const float fMod2     = filterLFO2.advance (numSamples);
    const float ampMod    = ampLFO.advance    (numSamples);
    const float breathMod = breathLFO.advance (numSamples);

    // Filter cutoff — modulated by sum of both filter LFOs.
    const float currentCutoff = filterBaseHz + fMod1 + fMod2;
    filterL.setCutoff (sr, currentCutoff);
    filterR.setCutoff (sr, currentCutoff);

    // Per-osc detune LFO → phase increment.
    for (auto& o : oscs)
    {
        const float lfoCents = o.detuneLFO.advance (numSamples);
        const float cents    = o.staticCents + lfoCents;
        o.phaseInc = detunedHz (o.baseHz, cents) / sr;
    }

    // Combined gain modulation: the standalone adds the amp + breath LFO
    // outputs to the master gain ramp. Since the envelope is applied as a
    // multiplier here, fold them into a `1 + ampMod + breathMod` scale.
    const float modScale = 1.0f + ampMod + breathMod;

    auto* const left  = outputBuffer.getWritePointer (0) + startSample;
    auto* const right = outputBuffer.getNumChannels() > 1
                            ? outputBuffer.getWritePointer (1) + startSample
                            : left;

    for (int s = 0; s < numSamples; ++s)
    {
        const float env = adsr.getNextSample();
        const float g   = env * modScale;

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

        left [s] += sumL * g;
        right[s] += sumR * g;
    }

    if (! adsr.isActive())
        clearCurrentNote();
}
