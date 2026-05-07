#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// Dark, warm, orange-accented LookAndFeel — mirrors the standalone web app.
// Rotary knobs draw a thin background ring + a thicker filled arc for the
// current value + a small tick at the thumb position.
class NorcoastLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr juce::uint32 kAccent      = 0xffc4915e;  // warm orange
    static constexpr juce::uint32 kAccentDim   = 0xff5b4530;
    static constexpr juce::uint32 kPanelBg     = 0xff111620;
    static constexpr juce::uint32 kPanelEdge   = 0xff1f2530;
    static constexpr juce::uint32 kText        = 0xffe6e1d8;
    static constexpr juce::uint32 kTextDim     = 0x99ffffff;
    static constexpr juce::uint32 kBg          = 0xff07090e;
    static constexpr juce::uint32 kBgMid       = 0xff0b0f1a;
    static constexpr juce::uint32 kBgEdge      = 0xff06080d;

    NorcoastLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (kAccent));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (kAccentDim));
        setColour (juce::Slider::thumbColourId,               juce::Colour (kAccent).brighter (0.3f));
        setColour (juce::Slider::textBoxTextColourId,         juce::Colour (kText));
        setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);

        setColour (juce::Label::textColourId,                 juce::Colour (kText));

        setColour (juce::TextButton::buttonColourId,          juce::Colour (kPanelBg));
        setColour (juce::TextButton::buttonOnColourId,        juce::Colour (kAccent));
        setColour (juce::TextButton::textColourOnId,          juce::Colour (kBg));
        setColour (juce::TextButton::textColourOffId,         juce::Colour (kAccent));

        setColour (juce::ComboBox::outlineColourId,           juce::Colour (kPanelEdge));
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        const float diameter = (float) juce::jmin (w, h) - 6.0f;
        const float radius   = diameter * 0.5f;
        const float cx       = (float) x + (float) w * 0.5f;
        const float cy       = (float) y + (float) h * 0.5f;
        const float thumbAngle = startAngle + pos * (endAngle - startAngle);

        const auto outline = slider.findColour (juce::Slider::rotarySliderOutlineColourId);
        const auto fill    = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // Background ring — thin, dim.
        {
            juce::Path bg;
            bg.addCentredArc (cx, cy, radius, radius, 0.0f,
                              startAngle, endAngle, true);
            g.setColour (outline);
            g.strokePath (bg, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
        }

        // Value arc — thicker, accent.
        {
            juce::Path arc;
            arc.addCentredArc (cx, cy, radius, radius, 0.0f,
                               startAngle, thumbAngle, true);
            g.setColour (fill);
            g.strokePath (arc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
        }

        // Centre dot — small accent dot, always visible.
        g.setColour (juce::Colour (kPanelEdge));
        g.fillEllipse (cx - radius * 0.55f, cy - radius * 0.55f,
                       radius * 1.1f, radius * 1.1f);

        // Thumb tick — short line from centre toward arc.
        juce::Path tick;
        tick.addRectangle (-1.5f, -radius * 0.85f, 3.0f, radius * 0.5f);
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillPath (tick, juce::AffineTransform::rotation (thumbAngle).translated (cx, cy));
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        // ~+2px vs the JUCE default — the small knob/fader captions
        // were uncomfortably tight to read on stage.
        return juce::FontOptions ((float) label.getHeight() * 0.85f).withStyle ("Regular");
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::FontOptions ((float) buttonHeight * 0.55f).withStyle ("Regular");
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& /*bg*/, bool over, bool down) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const auto onColour  = button.findColour (juce::TextButton::buttonOnColourId);
        const auto offColour = button.findColour (juce::TextButton::buttonColourId);
        const auto base = button.getToggleState()
            ? onColour
            : (down ? offColour.brighter (0.1f)
                    : (over ? offColour.brighter (0.05f) : offColour));
        g.setColour (base);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (juce::Colour (kPanelEdge));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }
};
