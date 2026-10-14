#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ScreenReaderAnnouncer.h"

namespace OpenTune::a11y {

// ComboBox that speaks the new item when it is changed from the keyboard.
// JUCE raises a UIA value change for a collapsed combo box, but NVDA does not
// speak it, so arrows changed "Pitch snap" silently. Also adds Home/End
// (first/last item), which the stock JUCE ComboBox ignores.
class AccessibleComboBox : public juce::ComboBox
{
public:
    using juce::ComboBox::ComboBox;

    bool keyPressed(const juce::KeyPress& key) override
    {
        const int before = getSelectedItemIndex();
        bool handled = false;

        if (key == juce::KeyPress::homeKey || key == juce::KeyPress::endKey)
        {
            // Walk from the edge so disabled items are skipped, like the arrows do.
            const bool fromStart = key == juce::KeyPress::homeKey;
            for (int i = fromStart ? 0 : getNumItems() - 1; i >= 0 && i < getNumItems(); i += fromStart ? 1 : -1)
            {
                if (isItemEnabled(getItemId(i)))
                {
                    setSelectedItemIndex(i);
                    break;
                }
            }
            handled = true;
        }
        else
        {
            handled = juce::ComboBox::keyPressed(key);
        }

        // Only speak a real change: at the edge of the list stay silent.
        if (getSelectedItemIndex() != before)
            announce(*this, getText());

        return handled;
    }
};

} // namespace OpenTune::a11y
