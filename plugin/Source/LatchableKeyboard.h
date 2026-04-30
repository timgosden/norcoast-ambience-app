#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// MidiKeyboardComponent variant with a latch mode:
//   - latch off: normal behaviour (note-on on press, note-off on release).
//   - latch on : click toggles the note. mouse-up does nothing, so chords
//                build up across multiple clicks.
class LatchableKeyboard : public juce::MidiKeyboardComponent
{
public:
    LatchableKeyboard (juce::MidiKeyboardState& s)
        : juce::MidiKeyboardComponent (s, juce::MidiKeyboardComponent::horizontalKeyboard),
          state (s) {}

    void setLatched (bool shouldLatch)
    {
        latched = shouldLatch;
        if (! latched)
            state.allNotesOff (getMidiChannel());
    }
    bool isLatched() const noexcept { return latched; }

protected:
    bool mouseDownOnKey (int midiNoteNumber, const juce::MouseEvent& e) override
    {
        if (latched)
        {
            const int ch = getMidiChannel();
            if (state.isNoteOn (ch, midiNoteNumber))
            {
                state.noteOff (ch, midiNoteNumber, 0.0f);
                return false; // suppress the default note-on
            }
            return true;      // let parent fire note-on; mouse-up won't release
        }
        return juce::MidiKeyboardComponent::mouseDownOnKey (midiNoteNumber, e);
    }

    void mouseUpOnKey (int midiNoteNumber, const juce::MouseEvent& e) override
    {
        if (latched) return;
        juce::MidiKeyboardComponent::mouseUpOnKey (midiNoteNumber, e);
    }

    bool mouseDraggedToKey (int midiNoteNumber, const juce::MouseEvent& e) override
    {
        // In latch mode, dragging across keys would toggle each one in turn —
        // that's almost never what the user wants. Suppress.
        if (latched) return false;
        return juce::MidiKeyboardComponent::mouseDraggedToKey (midiNoteNumber, e);
    }

private:
    juce::MidiKeyboardState& state;
    bool latched = false;
};
