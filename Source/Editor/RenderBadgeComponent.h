#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Standalone/UI/UIColors.h"

class RenderBadgeComponent : public juce::Component
{
public:
    RenderBadgeComponent()
    {
        setInterceptsMouseClicks(false, false);
        setAlwaysOnTop(true);
    }

    void setMessageText(const juce::String& text)
    {
        if (text_ != text) {
            text_ = text;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        if (text_.isEmpty()) return;

        auto area = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(juce::Colour(0xCC000000));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(juce::Colours::white);
        g.setFont(OpenTune::UIColors::getUIFontExact(13.0f));
        g.drawText(text_, area, juce::Justification::centred, false);
    }

    // Accessibility: readable by NVDA object navigation ("Rendering (2/5)") instead of an
    // unnamed object; hidden while empty. Not spoken by itself: that was too talkative.
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        class BadgeAccessibilityHandler final : public juce::AccessibilityHandler {
        public:
            explicit BadgeAccessibilityHandler(RenderBadgeComponent& owner)
                : juce::AccessibilityHandler(owner, juce::AccessibilityRole::staticText), owner_(owner) {}

            juce::String getTitle() const override { return owner_.text_; }

            juce::AccessibleState getCurrentState() const override
            {
                auto state = juce::AccessibilityHandler::getCurrentState();
                return owner_.text_.isEmpty() ? state.withIgnored() : state;
            }

        private:
            RenderBadgeComponent& owner_;
        };

        return std::make_unique<BadgeAccessibilityHandler>(*this);
    }

private:
    juce::String text_;
};
