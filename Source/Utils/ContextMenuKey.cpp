#include "ContextMenuKey.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace OpenTune::a11y {

bool isContextMenuKey(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    return key.getKeyCode() == juce::KeyPress::F10Key
        && mods.isShiftDown() && !mods.isCtrlDown() && !mods.isAltDown() && !mods.isCommandDown();
}

bool ApplicationsKeyWatcher::pressedNow()
{
#if JUCE_WINDOWS
    // juce::KeyPress::isKeyCurrentlyDown cannot ask for VK_APPS: 0x5D is translated as ']'.
    const bool down = (::GetAsyncKeyState(VK_APPS) & 0x8000) != 0;
    const bool pressed = down && !wasDown_;
    wasDown_ = down;
    return pressed;
#else
    return false;
#endif
}

} // namespace OpenTune::a11y
