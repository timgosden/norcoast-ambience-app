#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Single source of truth for all host-visible parameters.
// IDs match the keys used in the standalone's localStorage save format
// where reasonable, so future preset interchange stays straightforward.
namespace ParamID
{
    inline constexpr const char* foundationVol = "foundationVol";
    inline constexpr const char* padsVol       = "padsVol";

    inline constexpr const char* chorusMix     = "chorusMix";

    inline constexpr const char* delayMix      = "delayMix";
    inline constexpr const char* delayFb       = "delayFb";
    inline constexpr const char* delayTimeMs   = "delayTimeMs";
    inline constexpr const char* delayTone     = "delayTone";

    inline constexpr const char* reverbMix     = "reverbMix";
    inline constexpr const char* reverbSize    = "reverbSize";
    inline constexpr const char* reverbMod     = "reverbMod";

    inline constexpr const char* shimmerVol    = "shimmerVol";
    inline constexpr const char* widthMod      = "widthMod";
    inline constexpr const char* satAmt        = "satAmt";
    inline constexpr const char* masterVol     = "masterVol";

    inline constexpr const char* eqLow         = "eqLow";
    inline constexpr const char* eqLoMid       = "eqLoMid";
    inline constexpr const char* eqHiMid       = "eqHiMid";
    inline constexpr const char* eqHigh        = "eqHigh";
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

    return layout;
}
