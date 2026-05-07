#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "NorcoastLookAndFeel.h"

// 16-step × 3-voice clickable drum grid (kick / tom / hat). Reads/writes
// the drumCustomLo / drumCustomMd / drumCustomHh int params (16-bit
// masks) via APVTS so the processor's drum machine sees clicks
// immediately, and DAW automation / preset recall stays in sync.
//
// Click any cell to toggle that step. If the drum pattern is not
// 'Custom' at the moment of the click, the grid auto-switches it to
// Custom so the click takes effect immediately.
class StepSequencerGrid : public juce::Component, private juce::Timer
{
public:
    explicit StepSequencerGrid (juce::AudioProcessorValueTreeState& s)
        : apvts (s)
    {
        startTimerHz (15);
    }

    ~StepSequencerGrid() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelBg));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (juce::Colour (NorcoastLookAndFeel::kPanelEdge));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        // Voice row colours — same hues used in the standalone grid.
        const std::array<juce::Colour, 3> rowColours {{
            juce::Colour (0xffc4915e),  // lo  (kick) — warm orange
            juce::Colour (0xff5eb88a),  // md  (tom)  — green
            juce::Colour (0xff7eb6d4)   // hh  (hat)  — blue
        }};
        const std::array<juce::String, 3> rowLabels {{ "lo", "md", "hh" }};

        const int loMask  = (int) apvts.getRawParameterValue (ParamID::drumCustomLo)->load();
        const int mdMask  = (int) apvts.getRawParameterValue (ParamID::drumCustomMd)->load();
        const int hhMask  = (int) apvts.getRawParameterValue (ParamID::drumCustomHh)->load();
        const std::array<int, 3> masks { loMask, mdMask, hhMask };

        const bool isCustom = (int) apvts.getRawParameterValue (ParamID::drumPattern)->load() == 5;

        // Time signature: 4/4 → 16 steps grouped by 4; 6/8 → 12 steps
        // grouped by 3. Read once per repaint so the grid swaps live
        // when the user flips the toggle on the DRUMS strip.
        const bool is68    = (int) apvts.getRawParameterValue (ParamID::timeSig)->load() == 1;
        const int  nSteps  = is68 ? 12 : 16;
        const int  group   = is68 ? 3  : 4;

        const auto cells = getCellArea();
        const float labelW = (float) kLabelWidth;
        const float cellW  = (cells.getWidth() - labelW) / (float) nSteps;
        const float cellH  = cells.getHeight() / 3.0f;

        // Row labels
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        for (int row = 0; row < 3; ++row)
        {
            g.setColour (rowColours[(size_t) row].withAlpha (0.55f));
            g.drawText (rowLabels[(size_t) row],
                        juce::Rectangle<float> (cells.getX(), cells.getY() + row * cellH,
                                                labelW, cellH),
                        juce::Justification::centred);
        }

        // Cells
        for (int row = 0; row < 3; ++row)
        {
            for (int step = 0; step < nSteps; ++step)
            {
                const float x = cells.getX() + labelW + step * cellW;
                const float y = cells.getY() + row * cellH;
                const auto cellBox = juce::Rectangle<float> (x, y, cellW, cellH).reduced (1.5f, 2.5f);

                const bool on = (masks[(size_t) row] & (1 << step)) != 0;
                const auto baseCol = rowColours[(size_t) row];

                if (on)
                {
                    g.setColour (isCustom ? baseCol.withAlpha (0.85f)
                                          : baseCol.withAlpha (0.45f));   // dim if not Custom
                    g.fillRoundedRectangle (cellBox, 2.0f);
                }
                else
                {
                    g.setColour (juce::Colour (0xff1a1f29));
                    g.fillRoundedRectangle (cellBox, 2.0f);
                    g.setColour (juce::Colour (0xff242a36));
                    g.drawRoundedRectangle (cellBox, 2.0f, 0.5f);
                }

                // Beat divider — separates groups of 3 (6/8) or 4 (4/4).
                if (step > 0 && step % group == 0 && row == 0)
                {
                    g.setColour (juce::Colour (0x55ffffff));
                    g.drawVerticalLine ((int) x,
                                        cells.getY(), cells.getBottom());
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        toggleCellAt (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        // Drag-paint within a single mouse-down: only toggle a cell
        // when we cross into a new one. Tracked via lastDraggedCell.
        const auto target = cellAt (e.position);
        if (target.first < 0 || target == lastDraggedCell) return;
        lastDraggedCell = target;
        toggleBit (target.first, target.second);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        lastDraggedCell = { -1, -1 };
    }

private:
    void timerCallback() override { repaint(); }

    static constexpr int kLabelWidth = 26;

    juce::Rectangle<float> getCellArea() const
    {
        return getLocalBounds().toFloat().reduced (4.0f);
    }

    std::pair<int, int> cellAt (juce::Point<float> p) const
    {
        const auto cells = getCellArea();
        if (! cells.contains (p)) return { -1, -1 };
        const bool is68    = (int) apvts.getRawParameterValue (ParamID::timeSig)->load() == 1;
        const int  nSteps  = is68 ? 12 : 16;
        const float labelW = (float) kLabelWidth;
        const float cellW  = (cells.getWidth() - labelW) / (float) nSteps;
        const float cellH  = cells.getHeight() / 3.0f;
        const float x = p.x - cells.getX() - labelW;
        const float y = p.y - cells.getY();
        if (x < 0) return { -1, -1 };
        const int step = juce::jlimit (0, nSteps - 1, (int) (x / cellW));
        const int row  = juce::jlimit (0, 2,  (int) (y / cellH));
        return { step, row };
    }

    void toggleCellAt (juce::Point<float> p)
    {
        const auto target = cellAt (p);
        if (target.first < 0) return;
        lastDraggedCell = target;
        toggleBit (target.first, target.second);
    }

    void toggleBit (int step, int row)
    {
        static const char* const maskIDs[] {
            ParamID::drumCustomLo, ParamID::drumCustomMd, ParamID::drumCustomHh
        };
        const auto* paramID = maskIDs[(size_t) juce::jlimit (0, 2, row)];
        const int mask    = (int) apvts.getRawParameterValue (paramID)->load();
        const int newMask = mask ^ (1 << step);

        if (auto* p = apvts.getParameter (paramID))
        {
            const auto range = p->getNormalisableRange();
            p->setValueNotifyingHost (range.convertTo0to1 ((float) newMask));
        }

        // Auto-switch the drum pattern to 'Custom' so the click takes
        // effect (otherwise the grid is read-only).
        if (auto* patternParam = apvts.getParameter (ParamID::drumPattern))
        {
            const int currentPattern = (int) apvts.getRawParameterValue (ParamID::drumPattern)->load();
            if (currentPattern != 5)   // Custom = index 5
            {
                const auto range = patternParam->getNormalisableRange();
                patternParam->setValueNotifyingHost (range.convertTo0to1 (5.0f));
            }
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::pair<int, int> lastDraggedCell { -1, -1 };
};
