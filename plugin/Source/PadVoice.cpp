#include "PadVoice.h"

namespace
{
    // The standalone uses a fixed `spread = 0.5`, which collapses the formulas:
    //   detune cents = t * ds * spread * 2.5    →  t * ds * 1.25
    //   pan          = t * pan * (0.3 + spread * 0.7)
    //                                            →  t * pan * 0.65
    constexpr float kSpreadConst = 1.25f;
    constexpr float kPanConst    = 0.65f;

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

PadVoice::PadVoice (const LayerConfig& c,
                    std::atomic<float>* gainParam,
                    std::atomic<float>* extraOct,
                    std::atomic<float>* velSens,
                    std::atomic<float>* pbRange)
    : cfg (c),
      layerGainParam (gainParam),
      extraSubOctaveParam (extraOct),
      velocitySensParam (velSens),
      pitchBendRangeParam (pbRange)
{
    // When the optional extra-sub-octave param is wired up, allocate one
    // extra octave's worth of oscillators so we never reallocate on the
    // audio thread when the toggle flips.
    int oscsPerOctave = 0;
    for (auto& t : cfg.timbres) oscsPerOctave += t.count;
    const int extraOctaves = (extraSubOctaveParam != nullptr) ? 1 : 0;
    oscs.resize ((size_t) (oscsPerOctave * ((int) cfg.octaves.size() + extraOctaves)));
    activeOscCount = (int) oscs.size();
}

void PadVoice::startNote (int midiNoteNumber, float velocity,
                          juce::SynthesiserSound*, int)
{
    const auto sr = getSampleRate();
    auto& rng = juce::Random::getSystemRandom();

    const double rootHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

    // Build the effective octave list — append an extra sub-octave when
    // the optional toggle is on (only used for Foundation right now).
    std::array<int, 8> effOcts {};
    int numOcts = 0;
    int lowest = cfg.octaves[0];
    for (int oct : cfg.octaves)
    {
        effOcts[(size_t) numOcts++] = oct;
        lowest = juce::jmin (lowest, oct);
    }
    if (extraSubOctaveParam != nullptr && extraSubOctaveParam->load() > 0.5f
        && numOcts < (int) effOcts.size())
    {
        effOcts[(size_t) numOcts++] = lowest - 1;
    }

    int idx = 0;
    for (int oi = 0; oi < numOcts; ++oi)
    {
        const double octHz = rootHz * std::pow (2.0, (double) effOcts[(size_t) oi]);

        // Standalone alternates pan-flip per note index (`ni%2===0?1:-1`).
        // Within one voice we have one note, so we mirror that flip across
        // octaves — preserves the symmetric ping-ponging when both layers
        // play the same chord.
        const float panFlip = (oi % 2 == 0 ? 1.0f : -1.0f);

        for (auto& tb : cfg.timbres)
        {
            const Wavetable& wt = Waves::get (tb.wave);
            for (int vi = 0; vi < tb.count; ++vi)
            {
                const float t = tb.count > 1
                    ? ((float) vi / (float) (tb.count - 1)) * 2.0f - 1.0f
                    : 0.0f;

                Osc& o = oscs[(size_t) idx++];
                o.table       = &wt;
                o.gain        = tb.gain;
                o.phase       = rng.nextDouble();
                o.baseHz      = octHz;
                o.staticCents = t * tb.ds * kSpreadConst;
                o.phaseInc    = detunedHz (octHz, o.staticCents) / sr;
                setPan (o, t * tb.pan * panFlip * kPanConst);

                // Per-osc detune LFO — same formula as in the standalone with
                // spread=0.5 → depth scale 0.55.
                const double lfoRate  = 0.01 + rng.nextDouble() * 0.05;
                const float  lfoDepth = (0.2f + (float) rng.nextDouble() * 1.5f) * 0.55f;
                o.detuneLFO.setup (sr, lfoRate, lfoDepth, rng.nextDouble());
            }
        }
    }

    activeOscCount = idx;

    filterLFO1.setup (sr, cfg.fRate, cfg.fMod, rng.nextDouble());

    // filterLFO2: incommensurate rate, 45% depth.
    const double lfo2Rate = (double) cfg.fRate * (0.27 + rng.nextDouble() * 0.13);
    filterLFO2.setup (sr, lfo2Rate, cfg.fMod * 0.45f, rng.nextDouble());

    ampLFO.setup (sr, cfg.aRate, cfg.aMod, rng.nextDouble());

    // ~80 s breath cycle.
    const double breathRate = 0.011 + rng.nextDouble() * 0.006;
    breathLFO.setup (sr, breathRate, 0.03f, rng.nextDouble());

    filterBaseHz = cfg.fBase * 1.5f;
    filterL.setCutoff (sr, filterBaseHz);
    filterR.setCutoff (sr, filterBaseHz);
    filterL.reset();
    filterR.reset();

    adsr.setSampleRate (sr);
    adsr.setParameters (cfg.adsr);
    adsr.noteOn();

    // Velocity → per-voice gain. velocitySens=0 ignores velocity entirely
    // (ambient mode), velocitySens=1 makes vel=0 silent and vel=1 full.
    const float vSens = velocitySensParam != nullptr ? velocitySensParam->load() : 0.4f;
    velocityScale = (1.0f - vSens) + vSens * velocity;
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

    const float fMod1     = filterLFO1.advance (numSamples);
    const float fMod2     = filterLFO2.advance (numSamples);
    const float ampMod    = ampLFO.advance    (numSamples);
    const float breathMod = breathLFO.advance (numSamples);

    const float currentCutoff = filterBaseHz + fMod1 + fMod2;
    filterL.setCutoff (sr, currentCutoff);
    filterR.setCutoff (sr, currentCutoff);

    // Pitch bend: ± pitchBendRangeParam semitones around centre. Multiply
    // every oscillator's phase increment by 2^(bendSemis/12).
    const float pbRangeSemis = pitchBendRangeParam != nullptr
        ? pitchBendRangeParam->load() : 2.0f;
    const float bendCents = pitchBendNormalised * pbRangeSemis * 100.0f;

    for (int i = 0; i < activeOscCount; ++i)
    {
        auto& o = oscs[(size_t) i];
        const float lfoCents = o.detuneLFO.advance (numSamples);
        o.phaseInc = detunedHz (o.baseHz, o.staticCents + lfoCents + bendCents) / sr;
    }

    const float gainParam  = layerGainParam != nullptr ? layerGainParam->load() : 1.0f;
    const float layerScale = gainParam * velocityScale * (1.0f + ampMod + breathMod);

    auto* const left  = outputBuffer.getWritePointer (0) + startSample;
    auto* const right = outputBuffer.getNumChannels() > 1
                            ? outputBuffer.getWritePointer (1) + startSample
                            : left;

    for (int s = 0; s < numSamples; ++s)
    {
        const float env = adsr.getNextSample();
        const float g   = env * layerScale;

        float sumL = 0.0f, sumR = 0.0f;
        for (int i = 0; i < activeOscCount; ++i)
        {
            auto& o = oscs[(size_t) i];
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
