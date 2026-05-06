#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Single source of truth for all host-visible parameters.
// IDs match the keys used in the standalone's localStorage save format
// where reasonable, so future preset interchange stays straightforward.
namespace ParamID
{
    inline constexpr const char* foundationVol    = "foundationVol";
    inline constexpr const char* padsVol          = "padsVol";
    inline constexpr const char* textureVol       = "textureVol";
    inline constexpr const char* foundationSubOct = "foundationSubOct";
    inline constexpr const char* textureOctUp     = "textureOctUp";

    inline constexpr const char* chorusMix     = "chorusMix";

    inline constexpr const char* delayMix      = "delayMix";
    inline constexpr const char* delayFb       = "delayFb";
    inline constexpr const char* delayTimeMs   = "delayTimeMs";
    inline constexpr const char* delayTone     = "delayTone";

    inline constexpr const char* reverbMix     = "reverbMix";
    inline constexpr const char* reverbSize    = "reverbSize";
    inline constexpr const char* reverbMod     = "reverbMod";

    inline constexpr const char* lpfFreq       = "lpfFreq";
    inline constexpr const char* hpfFreq       = "hpfFreq";
    inline constexpr const char* shimmerVol    = "shimmerVol";
    inline constexpr const char* widthMod      = "widthMod";
    inline constexpr const char* satAmt        = "satAmt";
    inline constexpr const char* masterVol     = "masterVol";

    inline constexpr const char* eqLow         = "eqLow";
    inline constexpr const char* eqLoMid       = "eqLoMid";
    inline constexpr const char* eqHiMid       = "eqHiMid";
    inline constexpr const char* eqHigh        = "eqHigh";

    inline constexpr const char* arpVol        = "arpVol";
    inline constexpr const char* arpRate       = "arpRate";
    inline constexpr const char* arpOctaves    = "arpOctaves";
    inline constexpr const char* arpVoice      = "arpVoice";

    inline constexpr const char* drumVol       = "drumVol";
    inline constexpr const char* drumPattern   = "drumPattern";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using NormRange  = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto add = [&](auto p) { layout.add (std::move (p)); };

    // ─── Layer volumes ────────────────────────────────────────────────
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::foundationVol, 1 },
                                        "Foundation",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.55f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::padsVol, 1 },
                                        "Pads",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.50f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::textureVol, 1 },
                                        "Texture",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.85f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::foundationSubOct, 1 }, "Foundation Sub-Oct", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::textureOctUp, 1 }, "Texture +Oct", true));

    // ─── Chorus ───────────────────────────────────────────────────────
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::chorusMix, 1 },
                                        "Chorus",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.35f));

    // ─── Delay ────────────────────────────────────────────────────────
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::delayMix, 1 },
                                        "Delay Mix",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.46f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::delayFb, 1 },
                                        "Delay Feedback",
                                        NormRange { 0.0f, 0.95f, 0.001f }, 0.57f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::delayTimeMs, 1 },
                                        "Delay Time",
                                        NormRange { 50.0f, 2000.0f, 0.1f, 0.4f }, 857.0f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::delayTone, 1 },
                                        "Delay Tone",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 1.0f));

    // ─── Reverb ───────────────────────────────────────────────────────
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::reverbMix, 1 },
                                        "Reverb Mix",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.71f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::reverbSize, 1 },
                                        "Reverb Size",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.92f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::reverbMod, 1 },
                                        "Reverb Mod",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.74f));

    // Filter fader values are 0..1; the processor maps them through
    // lpfFaderToHz / hpfFaderToHz (port of the standalone helpers) to
    // get a logarithmic frequency curve.
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::lpfFreq, 1 },
                                        "Low Pass",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.897f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::hpfFreq, 1 },
                                        "High Pass",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.0f));

    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::shimmerVol, 1 },
                                        "Shimmer",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.07f));

    // ─── Master ───────────────────────────────────────────────────────
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::widthMod, 1 },
                                        "Width LFO",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.39f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::satAmt, 1 },
                                        "Saturation",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.0f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::masterVol, 1 },
                                        "Master",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.50f));

    // ─── EQ (dB) ──────────────────────────────────────────────────────
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::eqLow, 1 },
                                        "EQ Low",
                                        NormRange { -12.0f, 12.0f, 0.1f }, 0.0f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::eqLoMid, 1 },
                                        "EQ Lo-Mid",
                                        NormRange { -12.0f, 12.0f, 0.1f }, -0.6f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::eqHiMid, 1 },
                                        "EQ Hi-Mid",
                                        NormRange { -12.0f, 12.0f, 0.1f }, -2.0f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::eqHigh, 1 },
                                        "EQ High",
                                        NormRange { -12.0f, 12.0f, 0.1f }, 1.4f));

    // ─── Arpeggiator ──────────────────────────────────────────────────
    // arpVol: 0 = arp off
    // arpRate: note duration in beats (0.0625 = 16th, 0.125 = 8th note triplet,
    //                                   0.25 = 8th, 0.5 = quarter, 1.0 = half)
    // arpOctaves: 0..2 — extra octaves above the held notes
    // arpVoice: 0=Triangle, 1=Saw, 2=Sine
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::arpVol, 1 },
                                        "Arp Vol",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.0f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::arpRate, 1 },
                                        "Arp Rate",
                                        NormRange { 0.0625f, 1.0f, 0.001f, 0.5f }, 0.25f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::arpOctaves, 1 }, "Arp Octaves",
        juce::StringArray { "1", "2", "3" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::arpVoice, 1 }, "Arp Voice",
        juce::StringArray { "Triangle", "Saw", "Sine" }, 0));

    // ─── Drum machine ─────────────────────────────────────────────────
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::drumVol, 1 },
                                        "Drums",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::drumPattern, 1 }, "Drum Pattern",
        juce::StringArray { "Off", "Pulse", "Mist", "Stride", "Roam" }, 0));

    return layout;
}
