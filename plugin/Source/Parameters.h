#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Single source of truth for all host-visible parameters.
// IDs match the keys used in the standalone's localStorage save format
// where reasonable, so future preset interchange stays straightforward.
namespace ParamID
{
    inline constexpr const char* foundationVol    = "foundationVol";
    inline constexpr const char* padsVol          = "padsVol";
    inline constexpr const char* padsVol2         = "padsVol2";       // alt pads layer
    inline constexpr const char* textureVol       = "textureVol";
    inline constexpr const char* foundationSubOct = "foundationSubOct";
    inline constexpr const char* padsSubOct       = "padsSubOct";
    inline constexpr const char* pads2SubOct      = "pads2SubOct";
    inline constexpr const char* textureOctUp     = "textureOctUp";

    // Per-layer mutes for the gigging mixer surface. A bool gate that
    // doesn't disturb the fader value, so unmuting brings the layer back
    // at the level the player set.
    inline constexpr const char* foundationMute = "foundationMute";
    inline constexpr const char* padsMute       = "padsMute";
    inline constexpr const char* pads2Mute      = "pads2Mute";
    inline constexpr const char* textureMute    = "textureMute";
    inline constexpr const char* arpMute        = "arpMute";
    inline constexpr const char* drumMute       = "drumMute";

    inline constexpr const char* chorusMix     = "chorusMix";

    inline constexpr const char* delayMix      = "delayMix";
    inline constexpr const char* delayFb       = "delayFb";
    inline constexpr const char* delayDiv      = "delayDiv";
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

    inline constexpr const char* drumVol         = "drumVol";
    inline constexpr const char* drumPattern     = "drumPattern";
    inline constexpr const char* drumCustomLo    = "drumCustomLo";   // 16-bit mask
    inline constexpr const char* drumCustomMd    = "drumCustomMd";
    inline constexpr const char* drumCustomHh    = "drumCustomHh";
    inline constexpr const char* timeSig         = "timeSig";        // 0=4/4, 1=6/8
    inline constexpr const char* bpm             = "bpm";            // manual fallback BPM

    inline constexpr const char* chordType         = "chordType";
    inline constexpr const char* customChordMask   = "customChordMask";
    inline constexpr const char* enabledChordsMask = "enabledChordsMask";
    inline constexpr const char* evolveOn          = "evolveOn";
    inline constexpr const char* evolveBars    = "evolveBars";
    inline constexpr const char* droneOn       = "droneOn";
    inline constexpr const char* homeRoot      = "homeRoot";
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
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::padsVol2, 1 },
                                        "Pads 2",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.0f));
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::textureVol, 1 },
                                        "Texture",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.85f));

    // Per-layer mute gates. Independent from the volume fader so a
    // muted-then-unmuted layer returns at the same level.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::foundationMute, 1 }, "Foundation Mute", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::padsMute,       1 }, "Pads Mute",       false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::pads2Mute,      1 }, "Pads 2 Mute",     false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::textureMute,    1 }, "Texture Mute",    false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::arpMute,        1 }, "Arp Mute",        false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::drumMute,       1 }, "Drum Mute",       false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::foundationSubOct, 1 }, "Foundation Sub-Oct", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::padsSubOct, 1 }, "Pads Sub-Oct",   false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::pads2SubOct, 1 }, "Pads 2 Sub-Oct", false));
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
    // Delay time is BPM-locked to a tempo division. Index 5 = 1/4 by default.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::delayDiv, 1 }, "Delay Div",
        juce::StringArray { "1/32", "1/16", "1/16.", "1/8", "1/8.", "1/4", "1/4." }, 5));
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

    // Shimmer mix range — internally caps at 50% wet which the user
    // sees as 100% on the knob (display formatter scales ×200).
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::shimmerVol, 1 },
                                        "Shimmer",
                                        NormRange { 0.0f, 0.5f, 0.001f }, 0.0f));

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
    // Arp rate is BPM-locked to a tempo subdivision (was a free float).
    // Default 1/8 — gentle motion at 70 BPM.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::arpRate, 1 }, "Arp Rate",
        juce::StringArray { "1/16", "1/8", "1/4", "1/2", "1 bar" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::arpOctaves, 1 }, "Arp Octave",
        juce::StringArray { "-1 oct", "Mid", "+1 oct" }, 1));   // default Mid
    // Voice names match the latest standalone web-app labels.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::arpVoice, 1 }, "Arp Voice",
        juce::StringArray { "Bloom", "Hush", "Glow" }, 2));   // default Glow

    // ─── Drum machine ─────────────────────────────────────────────────
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::drumVol, 1 },
                                        "Drums",
                                        NormRange { 0.0f, 1.0f, 0.001f }, 0.0f));
    // "Off" stays in the param list for backwards compatibility with
    // saved presets, but the editor hides it from the pill row — the
    // Movement fader handles muting now. Default = Pulse (idx 1).
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::drumPattern, 1 }, "Drum Pattern",
        juce::StringArray { "Off", "Pulse", "Mist", "Stride", "Roam", "Custom" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::timeSig, 1 }, "Time Sig",
        juce::StringArray { "4/4", "6/8" }, 0));

    // Manual BPM — used by arp / drums / evolve when the host doesn't
    // provide a playhead BPM (e.g. Standalone). Default 70 matches the
    // web app's worship-tempo default.
    add (std::make_unique<FloatParam> (juce::ParameterID { ParamID::bpm, 1 },
                                        "BPM",
                                        NormRange { 30.0f, 200.0f, 1.0f }, 70.0f));

    // 16-step bitmasks for the Custom pattern, one per voice (lo / md / hh).
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamID::drumCustomLo, 1 }, "Drum Lo",  0, 65535, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamID::drumCustomMd, 1 }, "Drum Md",  0, 65535, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamID::drumCustomHh, 1 }, "Drum Hh",  0, 65535, 0));

    // ─── Evolve / chord generator ─────────────────────────────────────
    // Chord type names mirror the standalone's CHORD_TYPES table.
    // evolveOn cycles chordType through the enabled set every evolveRate
    // seconds, so the "ENTIRE POINT" (auto-changing chord shapes from a
    // single held root) works inside the plugin too.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::chordType, 1 }, "Chord Type",
        juce::StringArray { "5th", "Sus2", "Sus4", "Maj7th", "9th", "Custom" }, 1));
    // Bitmask of major-scale degrees for the Custom chord (bit 0 = root,
    // always implicit; bits 1..6 = 2nd…7th of the major scale).
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamID::customChordMask, 1 },
        "Custom Chord Mask", 0, 127, 0b1010100));
    // Which chord types the Evolve cycle picks from (6 bits, one per
    // chord type). Default = all six enabled.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamID::enabledChordsMask, 1 },
        "Evolve Pool", 0, 63, 63));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::evolveOn, 1 }, "Evolve", true));
    // Evolve in BARS — default 4 bars (the chord-change cadence is now
    // fixed UI-side, so the user doesn't need to fiddle with this).
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::evolveBars, 1 }, "Evolve Bars",
        juce::StringArray { "1", "2", "4", "8", "16", "32" }, 2));

    // Drone — ON by default so the synth auto-plays a chord on load
    // (matches the standalone web app's behaviour). When on, a single
    // root note is held forever; the chord evolver layers intervals on
    // top. Player MIDI just adds extra notes to the mix.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::droneOn, 1 }, "Drone", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::homeRoot, 1 }, "Home Root",
        juce::StringArray { "C", "Db", "D", "Eb", "E", "F",
                            "Gb", "G", "Ab", "A", "Bb", "B" }, 0));

    return layout;
}
