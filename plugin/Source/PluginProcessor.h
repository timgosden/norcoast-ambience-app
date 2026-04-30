#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "LayerConfig.h"
#include "DattorroReverb.h"

class NorcoastAmbienceProcessor : public juce::AudioProcessor
{
public:
    NorcoastAmbienceProcessor();
    ~NorcoastAmbienceProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Norcoast Ambience"; }
    bool acceptsMidi()   const override { return true; }
    bool producesMidi()  const override { return false; }
    bool isMidiEffect()  const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int  getNumPrograms()              override { return 1; }
    int  getCurrentProgram()           override { return 0; }
    void setCurrentProgram (int)       override {}
    const juce::String getProgramName (int)            override { return {}; }
    void changeProgramName (int, const juce::String&)  override {}

    void getStateInformation (juce::MemoryBlock&)   override;
    void setStateInformation (const void*, int)     override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

private:
    static constexpr int kVoicesPerLayer = 8;

    LayerConfig foundationConfig;
    LayerConfig padsConfig;

    juce::Synthesiser foundationSynth;
    juce::Synthesiser padsSynth;
    juce::MidiKeyboardState keyboardState;

    // ─── Master FX chain ──────────────────────────────────────────────
    // Order: synth → chorus → delay → reverb. Mirrors the standalone's
    // signal flow (`globalLPF2 → _chorBus → reverb / delay / dry sum`).

    // Juno-style stereo chorus: two short modulated delays panned L/R.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> chorusDelayL { 4096 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> chorusDelayR { 4096 };
    double chorusPhaseL = 0.0, chorusPhaseIncL = 0.0;
    double chorusPhaseR = 0.0, chorusPhaseIncR = 0.0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 1 << 18 };
    juce::dsp::IIR::Filter<float> delayFbLpfL, delayFbLpfR;   // 3 kHz LPF in feedback path
    juce::dsp::IIR::Filter<float> delayWetShelfL, delayWetShelfR; // +12 dB high-shelf on wet send

    DattorroReverb reverb;
    juce::AudioBuffer<float> reverbBuffer; // wet-only scratch

    // 4-band master EQ (post-reverb, matches the standalone tail):
    //   lowshelf   100 Hz  +0   dB
    //   peaking    350 Hz  -0.6 dB  Q 0.7
    //   peaking   2500 Hz  -2.0 dB  Q 0.9
    //   highshelf 8000 Hz  +1.4 dB
    using StereoIIR = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                     juce::dsp::IIR::Coefficients<float>>;
    StereoIIR eqLow, eqLoMid, eqHiMid, eqHigh;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorcoastAmbienceProcessor)
};
