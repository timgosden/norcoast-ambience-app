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
        // Default off-state button text is dim white — was kAccent
        // (orange) which made toggle states ambiguous because most
        // on-state buttons also use accent for their on colour. Now
        // toggling clearly switches dim-white → accent.
        setColour (juce::TextButton::textColourOffId,
                   juce::Colour (0xffd6dae6).withAlpha (0.55f));

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
        return juce::FontOptions ((float) label.getHeight() * 0.7f).withStyle ("Regular");
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::FontOptions ((float) buttonHeight * 0.5f).withStyle ("Regular");
    }

    // Web-app-style button: dark rounded card with a subtle border. The
    // toggled state does NOT fill the whole button (which felt heavy in
    // the plugin); instead the border + text colour shift to the on
    // colour, mirroring the standalone's pill-button look.
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& /*bg*/, bool over, bool down) override
    {
        const auto bounds   = button.getLocalBounds().toFloat().reduced (1.0f);
        const auto onColour = button.findColour (juce::TextButton::buttonOnColourId);

        // Dark slate fill — same shade for on/off so toggling doesn't
        // produce a visual jolt. Hover / down nudge the brightness a
        // touch so the affordance is still readable.
        const auto baseFill = juce::Colour (0xff141a26);
        const auto fill = down  ? baseFill.brighter (0.12f)
                       : over   ? baseFill.brighter (0.06f)
                                : baseFill;
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 8.0f);

        // Border: dim outline normally; on-state uses the accent colour
        // at full opacity so the active pill reads instantly. A 1.4 px
        // stroke gives the on state extra emphasis without filling.
        if (button.getToggleState())
        {
            g.setColour (onColour.withAlpha (0.85f));
            g.drawRoundedRectangle (bounds, 8.0f, 1.4f);
        }
        else
        {
            g.setColour (juce::Colour (0xff2a3142));
            g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
        }
    }

    // Pill-button text: when toggled, paint with the on-colour so the
    // border + text colour shift together. Off-state stays bright white-
    // ish so the label is always legible against the slate.
    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool /*over*/, bool /*down*/) override
    {
        g.setFont (getTextButtonFont (button, button.getHeight()));
        // Pull the button's own off-state colour so per-button accents
        // (e.g. the pink Stop pill) survive the LAF override.
        const auto on  = button.findColour (juce::TextButton::textColourOnId);
        const auto off = button.findColour (juce::TextButton::textColourOffId);
        // Use textColourOnId if it's set to something visible; otherwise
        // fall back to the buttonOnColourId so the active accent shows.
        const auto onResolved = (on.getAlpha() > 16
                                  && on != juce::Colour (kBg)
                                  && on != juce::Colour (kPanelBg))
            ? on
            : button.findColour (juce::TextButton::buttonOnColourId);
        g.setColour (button.getToggleState() ? onResolved : off);
        const int pad = juce::jmin (button.getHeight(), button.getWidth()) / 4;
        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().reduced (pad, 0),
                          juce::Justification::centred, 2);
    }

    // Linear slider: thin track with a prominent white-ish circular
    // thumb — matches the web app's gigging-friendly horizontal sliders.
    void drawLinearSlider (juce::Graphics& g,
                           int x, int y, int w, int h,
                           float sliderPos,
                           float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        // Stop-fade visual scrub for vertical faders — same logic as
        // before, just tweaks sliderPos toward the bottom of the track.
        if (style == juce::Slider::LinearVertical)
        {
            const auto prop = slider.getProperties()["stopFadeMult"];
            if (! prop.isVoid())
            {
                const float m = juce::jlimit (0.0f, 1.0f, (float) prop);
                if (m < 0.999f)
                {
                    const float range = maxSliderPos - minSliderPos;
                    if (std::abs (range) > 1e-3f)
                    {
                        const float n        = (sliderPos - minSliderPos) / range;
                        const float scaledN  = n * m;
                        sliderPos            = minSliderPos + scaledN * range;
                    }
                }
            }
        }

        if (style == juce::Slider::LinearHorizontal
         || style == juce::Slider::LinearVertical)
        {
            const float trackW = 3.0f;
            const auto bounds = juce::Rectangle<float> ((float) x, (float) y,
                                                         (float) w, (float) h);
            const auto fillCol  = slider.findColour (juce::Slider::rotarySliderFillColourId);
            const auto trackCol = juce::Colour (0xff2a3142);

            const bool horiz = style == juce::Slider::LinearHorizontal;
            const auto centreX = bounds.getCentreX();
            const auto centreY = bounds.getCentreY();

            juce::Rectangle<float> track;
            juce::Rectangle<float> filled;
            if (horiz)
            {
                track  = juce::Rectangle<float> (bounds.getX(),  centreY - trackW * 0.5f,
                                                 bounds.getWidth(), trackW);
                filled = juce::Rectangle<float> (bounds.getX(),  centreY - trackW * 0.5f,
                                                 sliderPos - bounds.getX(), trackW);
            }
            else
            {
                track  = juce::Rectangle<float> (centreX - trackW * 0.5f, bounds.getY(),
                                                 trackW, bounds.getHeight());
                // For vertical, sliderPos = top is max, bottom is min.
                filled = juce::Rectangle<float> (centreX - trackW * 0.5f, sliderPos,
                                                 trackW, bounds.getBottom() - sliderPos);
            }
            g.setColour (trackCol);
            g.fillRoundedRectangle (track, trackW * 0.5f);
            g.setColour (fillCol.withAlpha (0.85f));
            g.fillRoundedRectangle (filled, trackW * 0.5f);

            // White-ish thumb circle — sized so it reads at gigging
            // distance but doesn't dominate the strip. Drop a subtle
            // shadow ring so it pops on the dark slate.
            const float thumbR = horiz ? juce::jmin (10.0f, h * 0.48f)
                                       : juce::jmin (10.0f, w * 0.48f);
            const float tx = horiz ? sliderPos : centreX;
            const float ty = horiz ? centreY  : sliderPos;
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillEllipse (tx - thumbR - 0.5f, ty - thumbR + 1.0f,
                           thumbR * 2.0f + 1.0f, thumbR * 2.0f + 1.0f);
            g.setColour (juce::Colour (0xfff5f3ee));
            g.fillEllipse (tx - thumbR, ty - thumbR, thumbR * 2.0f, thumbR * 2.0f);
            return;
        }

        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h,
                                                 sliderPos, minSliderPos, maxSliderPos,
                                                 style, slider);
    }
};
