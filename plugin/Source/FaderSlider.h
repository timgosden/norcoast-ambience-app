#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// Linear-vertical slider with a stopFade multiplier. The actual slider
// value is untouched; paint() draws the thumb at value * stopFadeMult
// of the way up the track. Used by the editor to visually slide the
// 6 source-layer faders down to zero in lock-step with the audio Stop
// fade, without writing to the APVTS params (which would look like
// host automation).
class FaderSlider : public juce::Slider
{
public:
    float stopFadeMult = 1.0f;

    void paint (juce::Graphics& g) override
    {
        if (stopFadeMult >= 0.999f
         || getSliderStyle() != juce::Slider::LinearVertical)
        {
            juce::Slider::paint (g);
            return;
        }

        // Compute min / current / max thumb pixel positions ourselves
        // (matching JUCE's internal getLinearSliderPos), then scale
        // the current position toward the min by stopFadeMult.
        const auto bounds = getLocalBounds();
        const float trackTop    = (float) bounds.getY();
        const float trackBottom = (float) (bounds.getY() + bounds.getHeight());

        const double valueRange = getMaximum() - getMinimum();
        if (valueRange <= 0.0)
        {
            juce::Slider::paint (g);
            return;
        }

        // For LinearVertical, max value = top, min value = bottom.
        const float minPos = trackBottom;
        const float maxPos = trackTop;
        const float n      = (float) ((getValue() - getMinimum()) / valueRange);
        const float scaledN = n * juce::jlimit (0.0f, 1.0f, stopFadeMult);
        const float sliderPos = minPos + scaledN * (maxPos - minPos);

        getLookAndFeel().drawLinearSlider (g,
                                            bounds.getX(), bounds.getY(),
                                            bounds.getWidth(), bounds.getHeight(),
                                            sliderPos, minPos, maxPos,
                                            juce::Slider::LinearVertical, *this);
    }
};
