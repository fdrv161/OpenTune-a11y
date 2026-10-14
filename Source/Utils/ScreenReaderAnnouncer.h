#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Short spoken messages for NVDA, JAWS and Narrator (Windows, UI Automation)
// and VoiceOver (macOS).
namespace OpenTune::a11y {

enum class SpeechQueue
{
    replacePrevious, // a new message interrupts the previous one: navigation, values
    keepAll          // the message must not be lost: results, errors
};

// Speak text on behalf of the source component. It must be on screen and in the
// accessibility tree (usually the focused control). Title and description are not changed.
// Message thread only.
void announce(juce::Component& source, const juce::String& text,
              SpeechQueue queue = SpeechQueue::replacePrevious);

} // namespace OpenTune::a11y
