#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace OpenTune::a11y {

// Shift+F10: the standard Windows keyboard replacement for a right-click.
bool isContextMenuKey(const juce::KeyPress& key);

// The Applications (menu) key. JUCE treats it like a modifier key and never calls
// keyPressed for it, so call pressedNow() from keyStateChanged: it returns true once
// per press. Always false outside Windows.
class ApplicationsKeyWatcher
{
public:
    bool pressedNow();

private:
    bool wasDown_ = false;
};

} // namespace OpenTune::a11y
