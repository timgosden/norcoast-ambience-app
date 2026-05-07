#pragma once

#include <array>
#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"

// Pro-Q-style EQ curve display.
// Draws the combined frequency response of the four shelf/peak bands
// onto a log-frequency / dB grid. Drag a band node up/down to set its
// gain; double-click to reset it to 0 dB. The four band frequencies
// match the audio path (100 Hz · 350 Hz · 2.5 kHz · 8 kHz).
class EqCurveDisplay : public juce::Component,
                       private juce::Timer
{
public:
    EqCurveDisplay (juce::AudioProcessorValueTreeState& s)
        : apvts (s)
    {
        startTimerHz (24);
    }

    ~EqCurveDisplay() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);

        // Background panel.
        g.setColour (juce::Colour (0xff0d1220));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour (0xff242a36));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        // "MASTER EQ" header at top-left.
        g.setColour (juce::Colour (0xffb07acc));
        g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
        g.drawText ("GLOBAL EQ",
                    r.reduced (10.0f, 6.0f).withHeight (14.0f),
                    juce::Justification::topLeft);

        // Grid: vertical decade lines (100 Hz, 1 kHz, 10 kHz) and the
        // 0 dB centre line.
        g.setColour (juce::Colour (0xff242a36).withAlpha (0.7f));
        for (float fHz : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = r.getX() + freqToX (fHz, r.getWidth());
            g.drawVerticalLine ((int) x, r.getY() + 4.0f, r.getBottom() - 4.0f);
        }
        const float midY = r.getCentreY();
        g.setColour (juce::Colour (0xff2a3142));
        g.drawHorizontalLine ((int) midY, r.getX() + 4.0f, r.getRight() - 4.0f);
        // ±12 dB faint lines
        for (float dB : { -12.0f, 12.0f })
        {
            const float y = dBToY (dB, r);
            g.setColour (juce::Colour (0xff242a36));
            g.drawHorizontalLine ((int) y, r.getX() + 4.0f, r.getRight() - 4.0f);
        }

        // Decade labels along the bottom.
        g.setColour (juce::Colour (0xff8a93a8));
        g.setFont (juce::FontOptions (10.0f).withStyle ("Regular"));
        for (auto p : std::array<std::pair<float, juce::String>, 3> {{
            { 100.0f,   "100 Hz" },
            { 1000.0f,  "1 kHz"  },
            { 10000.0f, "10 kHz" } }})
        {
            const float x = r.getX() + freqToX (p.first, r.getWidth());
            g.drawText (p.second, juce::Rectangle<float> (x - 22.0f, r.getBottom() - 14.0f, 44.0f, 12.0f),
                        juce::Justification::centred);
        }

        // dB scale ticks on the right edge.
        g.setColour (juce::Colour (0xff596275));
        for (auto dB : { -12, -6, 0, 6, 12 })
        {
            const float y = dBToY ((float) dB, r);
            const juce::String txt = (dB > 0 ? "+" + juce::String (dB) : juce::String (dB));
            g.drawText (txt, juce::Rectangle<float> (r.getRight() - 36.0f, y - 7.0f, 32.0f, 12.0f),
                        juce::Justification::centredRight);
        }

        // Build coefficients from current param values (dB).
        const float dBLow   = apvts.getRawParameterValue (ParamID::eqLow)  ->load();
        const float dBLoMid = apvts.getRawParameterValue (ParamID::eqLoMid)->load();
        const float dBHiMid = apvts.getRawParameterValue (ParamID::eqHiMid)->load();
        const float dBHigh  = apvts.getRawParameterValue (ParamID::eqHigh) ->load();

        const auto cLow   = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (kSr,  100.0f, 0.7f, juce::Decibels::decibelsToGain (dBLow));
        const auto cLoMid = juce::dsp::IIR::Coefficients<float>::makePeakFilter (kSr,  350.0f, 1.0f, juce::Decibels::decibelsToGain (dBLoMid));
        const auto cHiMid = juce::dsp::IIR::Coefficients<float>::makePeakFilter (kSr, 2500.0f, 1.0f, juce::Decibels::decibelsToGain (dBHiMid));
        const auto cHigh  = juce::dsp::IIR::Coefficients<float>::makeHighShelf (kSr, 8000.0f, 0.7f, juce::Decibels::decibelsToGain (dBHigh));

        // Build the response curve sampled across the visible width.
        const int N = juce::jmax (8, (int) r.getWidth());
        juce::Path curve;
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) (N - 1);
            const double f = 20.0 * std::pow (1000.0, (double) t);
            double m = 1.0;
            m *= cLow  ->getMagnitudeForFrequency (f, kSr);
            m *= cLoMid->getMagnitudeForFrequency (f, kSr);
            m *= cHiMid->getMagnitudeForFrequency (f, kSr);
            m *= cHigh ->getMagnitudeForFrequency (f, kSr);
            const float dB = juce::Decibels::gainToDecibels ((float) m, -60.0f);
            const float x  = r.getX() + (float) i * r.getWidth() / (float) (N - 1);
            const float y  = dBToY (dB, r);
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }

        // Soft fill under the curve toward the centre line.
        juce::Path fill = curve;
        fill.lineTo (r.getRight(), midY);
        fill.lineTo (r.getX(),     midY);
        fill.closeSubPath();
        g.setColour (juce::Colour (0xffb07acc).withAlpha (0.15f));
        g.fillPath (fill);

        // Curve stroke.
        g.setColour (juce::Colour (0xffb07acc));
        g.strokePath (curve, juce::PathStrokeType (2.0f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        // Four draggable band nodes (with frequency tag underneath).
        const std::array<std::pair<float, juce::String>, 4> bandFreqs {{
            { 100.0f,  "100" },
            { 350.0f,  "350" },
            { 2500.0f, "2.5k" },
            { 8000.0f, "8k"  } }};
        const std::array<float, 4> dBs { dBLow, dBLoMid, dBHiMid, dBHigh };

        g.setFont (juce::FontOptions (10.0f).withStyle ("Regular"));
        for (size_t i = 0; i < bandFreqs.size(); ++i)
        {
            const float x = r.getX() + freqToX (bandFreqs[i].first, r.getWidth());
            const float y = dBToY (dBs[i], r);

            // Node circle — bigger when hovered or dragged so the
            // affordance is obvious.
            const bool active = ((int) i == draggingBand) || ((int) i == hoverBand);
            const float nodeR = active ? 8.0f : 6.0f;
            g.setColour (juce::Colour (0xff0d1220));
            g.fillEllipse (x - nodeR, y - nodeR, nodeR * 2.0f, nodeR * 2.0f);
            g.setColour (juce::Colour (0xffb07acc));
            g.drawEllipse (x - nodeR, y - nodeR, nodeR * 2.0f, nodeR * 2.0f,
                           active ? 2.5f : 2.0f);
            if ((int) i == draggingBand)
            {
                g.setColour (juce::Colour (0xffe6c79c));
                g.fillEllipse (x - 3, y - 3, 6, 6);
            }

            // Frequency tag below the band node — small, dim.
            g.setColour (juce::Colour (0xff596275));
            g.drawText (bandFreqs[i].second,
                        juce::Rectangle<float> (x - 22.0f, y + nodeR + 2.0f, 44.0f, 12.0f),
                        juce::Justification::centred);

            // dB readout floating above the node while dragging.
            if ((int) i == draggingBand)
            {
                const juce::String dbTxt = (dBs[i] >= 0.0f ? "+" : "")
                                           + juce::String (dBs[i], 1) + " dB";
                g.setColour (juce::Colour (0xffe6c79c));
                g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
                g.drawText (dbTxt,
                            juce::Rectangle<float> (x - 36.0f, y - nodeR - 18.0f, 72.0f, 14.0f),
                            juce::Justification::centred);
                g.setFont (juce::FontOptions (10.0f).withStyle ("Regular"));
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        draggingBand = nearestBandIndex ((float) e.x);
        applyBandFromMouse (draggingBand, (float) e.y);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingBand < 0) return;
        applyBandFromMouse (draggingBand, (float) e.y);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        draggingBand = -1;
        repaint();
    }

    // Hover state — change the cursor over a band node and highlight it
    // so the user can see they're draggable.
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int idx = nearestBandIndex ((float) e.x);
        if (idx != hoverBand)
        {
            hoverBand = idx;
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            repaint();
        }
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverBand != -1) { hoverBand = -1; repaint(); }
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int idx = nearestBandIndex ((float) e.x);
        if (auto* p = apvts.getParameter (paramIdAt (idx)))
        {
            const auto range = p->getNormalisableRange();
            p->setValueNotifyingHost (range.convertTo0to1 (0.0f));   // 0 dB
        }
    }

private:
    void timerCallback() override { repaint(); }

    static constexpr double kSr = 48000.0;       // analysis-only sample rate

    static float freqToX (float fHz, float w) noexcept
    {
        // Log scale 20 Hz → 20 kHz.
        return std::log (fHz / 20.0f) / std::log (1000.0f) * w;
    }

    static float dBToY (float dB, juce::Rectangle<float> r) noexcept
    {
        return juce::jmap (juce::jlimit (-15.0f, 15.0f, dB),
                           -15.0f, 15.0f,
                           r.getBottom() - 6.0f, r.getY() + 6.0f);
    }

    int nearestBandIndex (float mouseX) const
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        const std::array<float, 4> freqs { 100.0f, 350.0f, 2500.0f, 8000.0f };
        int   bestIdx = 0;
        float bestDist = 1.0e9f;
        for (int i = 0; i < 4; ++i)
        {
            const float bx = r.getX() + freqToX (freqs[(size_t) i], r.getWidth());
            const float d  = std::abs (bx - mouseX);
            if (d < bestDist) { bestDist = d; bestIdx = i; }
        }
        return bestIdx;
    }

    void applyBandFromMouse (int idx, float mouseY)
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float dB = juce::jmap (mouseY,
                                     r.getY() + 6.0f, r.getBottom() - 6.0f,
                                     15.0f, -15.0f);
        const float clamped = juce::jlimit (-12.0f, 12.0f, dB);
        if (auto* p = apvts.getParameter (paramIdAt (idx)))
        {
            const auto range = p->getNormalisableRange();
            p->setValueNotifyingHost (range.convertTo0to1 (clamped));
        }
    }

    static const char* paramIdAt (int idx) noexcept
    {
        switch (idx)
        {
            case 0: return ParamID::eqLow;
            case 1: return ParamID::eqLoMid;
            case 2: return ParamID::eqHiMid;
            default: return ParamID::eqHigh;
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    int draggingBand = -1;
    int hoverBand    = -1;
};
