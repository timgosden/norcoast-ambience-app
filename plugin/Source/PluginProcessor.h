#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

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
    double getTailLengthSeconds() const override { return 8.0; } // matches longest release

    int  getNumPrograms()              override { return 1; }
    int  getCurrentProgram()           override { return 0; }
    void setCurrentProgram (int)       override {}
    const juce::String getProgramName (int)            override { return {}; }
    void changeProgramName (int, const juce::String&)  override {}

    void getStateInformation (juce::MemoryBlock&)   override;
    void setStateInformation (const void*, int)     override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    // Exposed so the editor can render an on-screen MidiKeyboardComponent
    // that injects notes into the same MIDI stream the synth consumes.
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

private:
    static constexpr int kMaxVoices = 8;

    juce::Synthesiser synth;
    juce::MidiKeyboardState keyboardState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorcoastAmbienceProcessor)
};
