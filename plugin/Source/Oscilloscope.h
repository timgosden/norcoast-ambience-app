#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <array>

// Lock-free stereo oscilloscope. The audio thread `push`es samples into
// a ring buffer; the GUI thread snapshots them on a timer and draws.
class Oscilloscope : public juce::Component, private juce::Timer
{
public:
    static constexpr int kRingSize  = 1 << 12;   // 4096 samples
    static constexpr int kViewSize  = 1024;

    Oscilloscope()
    {
        startTimerHz (30);
    }

    ~Oscilloscope() override { stopTimer(); }

    // Audio thread.
    void pushBlock (const float* L, const float* R, int n) noexcept
    {
        int wpos = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            ringL[(size_t) wpos] = L[i];
            ringR[(size_t) wpos] = R[i];
            wpos = (wpos + 1) & (kRingSize - 1);
        }
        writePos.store (wpos, std::memory_order_release);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

        // Panel background
        g.setColour (juce::Colour (0xff111620));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (juce::Colour (0xff1f2530));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        // Centre line
        g.setColour (juce::Colour (0x33ffffff));
        g.drawHorizontalLine ((int) bounds.getCentreY(),
                              bounds.getX() + 8.0f, bounds.getRight() - 8.0f);

        // Snapshot — the audio thread might race a few samples but for a
        // visualisation this is fine.
        const int wpos = writePos.load (std::memory_order_acquire);
        const int start = (wpos - kViewSize + kRingSize) & (kRingSize - 1);

        const float w     = bounds.getWidth() - 16.0f;
        const float h     = bounds.getHeight() * 0.45f;
        const float cy    = bounds.getCentreY();
        const float xStep = w / (float) (kViewSize - 1);
        const float xOff  = bounds.getX() + 8.0f;

        auto drawTrace = [&] (const std::array<float, kRingSize>& ring,
                              juce::Colour colour, float yOffset)
        {
            juce::Path p;
            for (int i = 0; i < kViewSize; ++i)
            {
                const float v = ring[(size_t) ((start + i) & (kRingSize - 1))];
                const float y = cy + yOffset + juce::jlimit (-h * 0.45f, h * 0.45f, v * h);
                if (i == 0) p.startNewSubPath (xOff, y);
                else        p.lineTo (xOff + i * xStep, y);
            }
            g.setColour (colour);
            g.strokePath (p, juce::PathStrokeType (1.2f));
        };

        // Left = warm orange, Right = soft cyan, slight Y offset to show both.
        drawTrace (ringL, juce::Colour (0xc4c4915e), -2.0f);
        drawTrace (ringR, juce::Colour (0x999ec5d4),  2.0f);
    }

private:
    void timerCallback() override { repaint(); }

    std::array<float, kRingSize> ringL {};
    std::array<float, kRingSize> ringR {};
    std::atomic<int> writePos { 0 };
};
