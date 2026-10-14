#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace OpenTune::a11y {

// Knob for the keyboard, with the same keys as the EQ knobs: arrows step, Alt is a fine
// step, Page Up/Down a big one, Home = maximum and End = minimum (the user's rule).
// The stock JUCE slider moves by its tiny interval (0.01) and has no Home/End.
//
// Changes from the keys reach onCommit once the keys stop, so a held arrow is one undo
// step and one render; mouse changes reach it at once. Use onCommit, not onValueChange.
class AccessibleKnob : public juce::Slider, private juce::Timer
{
public:
    AccessibleKnob()
    {
        setWantsKeyboardFocus(true);
        // The value box is an editable Label: as a Tab stop it opened a text field
        // ("Retune Speed edit selected 15.00%"). A focus container keeps its children
        // out of the Tab order; the mouse can still click the box and type.
        setFocusContainerType(FocusContainerType::keyboardFocusContainer);
    }

    ~AccessibleKnob() override { stopTimer(); }

    void setKeySteps(double normal, double fine, double big)
    {
        step_ = normal;
        fineStep_ = fine;
        bigStep_ = big;
    }

    std::function<void()> onCommit;

    bool keyPressed(const juce::KeyPress& key) override
    {
        const auto mods = key.getModifiers();
        if (mods.isCtrlDown() || mods.isCommandDown() || mods.isShiftDown())
            return juce::Slider::keyPressed(key);

        const int code = key.getKeyCode();
        const double small = mods.isAltDown() ? fineStep_ : step_;
        double target = getValue();
        if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey)
            target += small;
        else if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey)
            target -= small;
        else if (code == juce::KeyPress::pageUpKey)
            target += bigStep_;
        else if (code == juce::KeyPress::pageDownKey)
            target -= bigStep_;
        else if (code == juce::KeyPress::homeKey)
            target = getMaximum();
        else if (code == juce::KeyPress::endKey)
            target = getMinimum();
        else
            return juce::Slider::keyPressed(key);

        target = juce::jlimit(getMinimum(), getMaximum(), target);
        // At the edge the value does not change and NVDA says nothing.
        if (target != getValue())
        {
            keyboardChange_ = true;
            // NVDA speaks the new value of the focused slider itself (JUCE value change event).
            setValue(target, juce::sendNotificationSync);
            keyboardChange_ = false;
        }
        return true;
    }

    void focusLost(FocusChangeType cause) override
    {
        commitPending();
        juce::Slider::focusLost(cause);
    }

private:
    void valueChanged() override
    {
        if (keyboardChange_)
        {
            startTimer(400);
            return;
        }
        stopTimer();
        if (onCommit)
            onCommit();
    }

    void timerCallback() override
    {
        stopTimer();
        if (onCommit)
            onCommit();
    }

    void commitPending()
    {
        if (isTimerRunning())
            timerCallback();
    }

    double step_ = 1.0;
    double fineStep_ = 0.1;
    double bigStep_ = 10.0;
    bool keyboardChange_ = false;
};

} // namespace OpenTune::a11y
