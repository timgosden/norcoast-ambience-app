#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "LayerConfig.h"
#include "DattorroReverb.h"
#include "Shimmer.h"
#include "Arpeggiator.h"
#include "DrumMachine.h"
#include "Texture.h"

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
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

private:
    static constexpr int kVoicesPerLayer = 8;

    LayerConfig foundationConfig;
    LayerConfig padsConfig;

    juce::AudioProcessorValueTreeState apvts;

    // Cached parameter atom pointers — read once per block in processBlock.
    std::atomic<float>* foundationVolParam    = nullptr;
    std::atomic<float>* padsVolParam          = nullptr;
    std::atomic<float>* textureVolParam       = nullptr;
    std::atomic<float>* foundationSubOctParam = nullptr;
    std::atomic<float>* textureOctUpParam     = nullptr;
    std::atomic<float>* chorusMixParam     = nullptr;
    std::atomic<float>* delayMixParam      = nullptr;
    std::atomic<float>* delayFbParam       = nullptr;
    std::atomic<float>* delayTimeMsParam   = nullptr;
    std::atomic<float>* delayToneParam     = nullptr;
    std::atomic<float>* reverbMixParam     = nullptr;
    std::atomic<float>* reverbSizeParam    = nullptr;
    std::atomic<float>* reverbModParam     = nullptr;
    std::atomic<float>* lpfFreqParam       = nullptr;
    std::atomic<float>* hpfFreqParam       = nullptr;
    std::atomic<float>* shimmerVolParam    = nullptr;
    std::atomic<float>* widthModParam      = nullptr;
    std::atomic<float>* satAmtParam        = nullptr;
    std::atomic<float>* masterVolParam     = nullptr;
    std::atomic<float>* eqLowParam         = nullptr;
    std::atomic<float>* eqLoMidParam       = nullptr;
    std::atomic<float>* eqHiMidParam       = nullptr;
    std::atomic<float>* eqHighParam        = nullptr;
    std::atomic<float>* arpVolParam        = nullptr;
    std::atomic<float>* arpRateParam       = nullptr;
    std::atomic<float>* arpOctavesParam    = nullptr;
    std::atomic<float>* arpVoiceParam      = nullptr;
    std::atomic<float>* drumVolParam       = nullptr;
    std::atomic<float>* drumPatternParam   = nullptr;

    juce::Synthesiser foundationSynth;
    juce::Synthesiser padsSynth;
    juce::MidiKeyboardState keyboardState;

    // ─── Master FX chain ──────────────────────────────────────────────
    // Order: synth → chorus → delay → reverb → width LFO → saturation
    //              → EQ → master gain.

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> chorusDelayL { 4096 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> chorusDelayR { 4096 };
    double chorusPhaseL = 0.0, chorusPhaseIncL = 0.0;
    double chorusPhaseR = 0.0, chorusPhaseIncR = 0.0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 1 << 18 };
    juce::dsp::IIR::Filter<float> delayFbLpfL, delayFbLpfR;
    juce::dsp::IIR::Filter<float> delayWetShelfL, delayWetShelfR;
    float lastDelayToneDb = -1000.0f;

    DattorroReverb reverb;
    juce::AudioBuffer<float> reverbBuffer;

    Shimmer shimmer;
    Arpeggiator arpeggiator;
    DrumMachine drumMachine;
    Texture     texture;
    std::vector<int> heldNotesScratch;   // reused across processBlock calls
    float lastReverbSize = -1.0f;
    float lastReverbMod  = -1.0f;

    double widthPhase = 0.0;
    double widthPhaseInc = 0.0;

    using StereoIIR = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                     juce::dsp::IIR::Coefficients<float>>;
    StereoIIR masterLpf, masterHpf;
    float lastLpfHz = -1.0f, lastHpfHz = -1.0f;

    StereoIIR eqLow, eqLoMid, eqHiMid, eqHigh;
    float lastEqLowDb = -1000.0f, lastEqLoMidDb = -1000.0f,
          lastEqHiMidDb = -1000.0f, lastEqHighDb = -1000.0f;

    // Smoothed master gain so big fader moves don't pop.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorcoastAmbienceProcessor)
};
