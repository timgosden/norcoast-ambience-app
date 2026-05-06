#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include "Parameters.h"

// Factory presets — each is a list of {paramID, value} pairs applied via
// APVTS to set the plugin into a starting point. Designed to give the
// player a few quick "moods" without having to build them from scratch.
namespace Presets
{
    struct Setting { const char* id; float value; };

    struct Preset
    {
        const char* name;
        std::vector<Setting> settings;
    };

    inline const std::vector<Preset>& factory()
    {
        static const std::vector<Preset> all =
        {
            {
                "Default",
                {
                    { ParamID::foundationVol,    0.55f },
                    { ParamID::padsVol,          0.50f },
                    { ParamID::textureVol,       0.85f },
                    { ParamID::foundationSubOct, 1.0f  },
                    { ParamID::textureOctUp,     1.0f  },
                    { ParamID::chorusMix,        0.35f },
                    { ParamID::delayMix,         0.46f },
                    { ParamID::delayFb,          0.57f },
                    { ParamID::delayDiv,         5.0f },         // 1/4
                    { ParamID::delayTone,        1.0f  },
                    { ParamID::reverbMix,        0.71f },
                    { ParamID::reverbSize,       0.92f },
                    { ParamID::reverbMod,        0.74f },
                    { ParamID::shimmerVol,       0.07f },
                    { ParamID::widthMod,         0.39f },
                    { ParamID::satAmt,           0.0f  },
                    { ParamID::masterVol,        0.50f },
                    { ParamID::lpfFreq,          0.897f },
                    { ParamID::hpfFreq,          0.0f  },
                    { ParamID::eqLow,            0.0f  },
                    { ParamID::eqLoMid,         -0.6f  },
                    { ParamID::eqHiMid,         -2.0f  },
                    { ParamID::eqHigh,           1.4f  },
                    { ParamID::arpVol,           0.0f  },
                    { ParamID::drumVol,          0.0f  }
                }
            },
            {
                "Calm",
                {
                    { ParamID::foundationVol,    0.6f  },
                    { ParamID::padsVol,          0.55f },
                    { ParamID::textureVol,       0.4f  },
                    { ParamID::foundationSubOct, 1.0f  },
                    { ParamID::reverbMix,        0.85f },
                    { ParamID::reverbSize,       0.95f },
                    { ParamID::shimmerVol,       0.04f },
                    { ParamID::delayMix,         0.3f  },
                    { ParamID::delayFb,          0.45f },
                    { ParamID::lpfFreq,          0.75f },
                    { ParamID::eqHigh,          -1.5f  },
                    { ParamID::widthMod,         0.5f  },
                    { ParamID::masterVol,        0.55f },
                    { ParamID::arpVol,           0.0f  },
                    { ParamID::drumVol,          0.0f  }
                }
            },
            {
                "Bright",
                {
                    { ParamID::foundationVol,    0.4f  },
                    { ParamID::padsVol,          0.65f },
                    { ParamID::textureVol,       0.7f  },
                    { ParamID::textureOctUp,     1.0f  },
                    { ParamID::shimmerVol,       0.18f },
                    { ParamID::reverbMix,        0.65f },
                    { ParamID::reverbSize,       0.88f },
                    { ParamID::reverbMod,        0.85f },
                    { ParamID::eqHigh,           3.0f  },
                    { ParamID::eqHiMid,          0.0f  },
                    { ParamID::lpfFreq,          1.0f  },
                    { ParamID::chorusMix,        0.45f },
                    { ParamID::widthMod,         0.55f },
                    { ParamID::masterVol,        0.50f },
                    { ParamID::arpVol,           0.0f  },
                    { ParamID::drumVol,          0.0f  }
                }
            },
            {
                "Dark",
                {
                    { ParamID::foundationVol,    0.7f  },
                    { ParamID::padsVol,          0.45f },
                    { ParamID::textureVol,       0.3f  },
                    { ParamID::foundationSubOct, 1.0f  },
                    { ParamID::shimmerVol,       0.0f  },
                    { ParamID::reverbMix,        0.60f },
                    { ParamID::reverbSize,       0.85f },
                    { ParamID::lpfFreq,          0.55f },
                    { ParamID::hpfFreq,          0.05f },
                    { ParamID::eqLow,            2.0f  },
                    { ParamID::eqHigh,          -3.0f  },
                    { ParamID::satAmt,           0.25f },
                    { ParamID::widthMod,         0.3f  },
                    { ParamID::masterVol,        0.55f },
                    { ParamID::arpVol,           0.0f  },
                    { ParamID::drumVol,          0.0f  }
                }
            },
            {
                "Movement",
                {
                    { ParamID::foundationVol,    0.5f  },
                    { ParamID::padsVol,          0.5f  },
                    { ParamID::textureVol,       0.3f  },
                    { ParamID::reverbMix,        0.65f },
                    { ParamID::shimmerVol,       0.05f },
                    { ParamID::arpVol,           0.4f  },
                    { ParamID::arpRate,          0.25f },     // 8th notes
                    { ParamID::arpOctaves,       1.0f  },
                    { ParamID::arpVoice,         0.0f  },     // triangle
                    { ParamID::drumVol,          0.5f  },
                    { ParamID::drumPattern,      3.0f  },     // Stride
                    { ParamID::widthMod,         0.5f  },
                    { ParamID::masterVol,        0.50f }
                }
            }
        };
        return all;
    }

    inline void apply (juce::AudioProcessorValueTreeState& apvts, const Preset& p)
    {
        for (const auto& s : p.settings)
            if (auto* param = apvts.getParameter (s.id))
            {
                const auto range = param->getNormalisableRange();
                const float normalised = range.convertTo0to1 (s.value);
                param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
            }
    }
}
