#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// MidiKeyboardComponent variant with:
//   - latch mode: clicking a key toggles it. Mouse-up does nothing while
//     latched, so chords build up across multiple clicks.
//   - computer-keyboard input enabled: the home row (a/s/d/f...) plays
//     notes from the configured base octave. Works whenever the editor
//     has keyboard focus.
//
// The latch override replaces mouseDown/mouseUp directly rather than
// going through mouseDownOnKey/mouseUpOnKey — the latter pair didn't
// reliably suppress note-offs in JUCE 8.
class LatchableKeyboard : public juce::MidiKeyboardComponent
{
public:
    LatchableKeyboard (juce::MidiKeyboardState& s)
        : juce::MidiKeyboardComponent (s, juce::MidiKeyboardComponent::horizontalKeyboard),
          state (s)
    {
        setWantsKeyboardFocus (true);
        setKeyPressBaseOctave (4);          // home row plays C4 / D4 / E4 …
    }

    void setLatched (bool shouldLatch)
    {
        latched = shouldLatch;
        if (! latched)
            state.allNotesOff (getMidiChannel());
    }
    bool isLatched() const noexcept { return latched; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! latched)
        {
            juce::MidiKeyboardComponent::mouseDown (e);
            return;
        }

        float vel = 0.7f;
        const int note = xyToNote (e.position, vel);
        if (note < 0) return;

        const int ch = getMidiChannel();
        if (state.isNoteOn (ch, note))
            state.noteOff (ch, note, 0.0f);
        else
            state.noteOn  (ch, note, juce::jmax (0.4f, vel));
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (latched) return;                // notes never release while latched
        juce::MidiKeyboardComponent::mouseUp (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (latched) return;                // drag-toggle disabled when latched
        juce::MidiKeyboardComponent::mouseDrag (e);
    }

private:
    juce::MidiKeyboardState& state;
    bool latched = false;
};
