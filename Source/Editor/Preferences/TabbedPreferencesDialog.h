#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <utility>
#include <vector>

#include "Utils/LocalizationManager.h"
#include "Utils/ScreenReaderAnnouncer.h"
#include "Standalone/UI/AuroraLookAndFeel.h"
#include "Standalone/UI/OpenTuneLookAndFeel.h"
#include "Standalone/UI/UIColors.h"

namespace OpenTune {

// 包装页面使其可滚动：Viewport 填满 tab 区域，内部页面保持内容高度
class ScrollablePage : public juce::Viewport
{
public:
    explicit ScrollablePage(std::unique_ptr<juce::Component> content, int contentHeight)
    {
        setScrollBarThickness(8);
        // Доступность: сама прокручиваемая область брала фокус по Tab (NVDA: «unknown»),
        // а полосы прокрутки работают только мышью. Элементы страницы остаются доступны.
        setWantsKeyboardFocus(false);
        getVerticalScrollBar().setAccessible(false);
        getHorizontalScrollBar().setAccessible(false);
        // setViewedComponent 将 content 放入内部 contentHolder
        // content 保持自身高度，viewport 自动提供滚动
        contentHeight_ = contentHeight;
        content_ = std::move(content);
        setViewedComponent(content_.get(), false);
        content_->setSize(600, contentHeight); // 初始宽度，resized 时会更新
    }

    void resized() override
    {
        juce::Viewport::resized();
        // viewport 宽度变化时，同步更新内容宽度（高度保持不变以支持滚动）
        if (content_ != nullptr)
            content_->setSize(getWidth(), contentHeight_);
    }

private:
    std::unique_ptr<juce::Component> content_;
    int contentHeight_ = 0;
};

class TabbedPreferencesDialog : public juce::Component
{
public:
    struct PageSpec {
        juce::String title;
        std::unique_ptr<juce::Component> content;
        int totalHeight = 0; // 内容总高度，用于判断是否需要滚动
    };

    explicit TabbedPreferencesDialog(std::vector<PageSpec> pages)
    {
        applyCurrentLookAndFeel();

        addAndMakeVisible(tabbedComponent_);
        tabbedComponent_.setColour(juce::TabbedComponent::backgroundColourId, UIColors::backgroundDark);
        tabbedComponent_.setColour(juce::TabbedButtonBar::tabOutlineColourId, UIColors::panelBorder);
        tabbedComponent_.setTabBarDepth(32);

        for (auto& page : pages) {
            jassert(page.content != nullptr);
            if (page.content == nullptr) {
                continue;
            }

            nameControlsByPrecedingLabels(*page.content);

            // 统一包装为可滚动：内容比视口矮时滚动条不出现，比视口高时自动出现
            auto* scrollable = new ScrollablePage(std::move(page.content), page.totalHeight);
            tabbedComponent_.addTab(page.title, UIColors::backgroundDark, scrollable, true);
        }

        // Доступность: кнопки вкладок по умолчанию не берут фокус, и Tab их пропускал.
        auto& tabBar = tabbedComponent_.getTabbedButtonBar();
        for (int i = 0; i < tabBar.getNumTabs(); ++i)
            if (auto* tabButton = tabBar.getTabButton(i))
                tabButton->setWantsKeyboardFocus(true);
        tabbedComponent_.onTabChanged = [this](const juce::String& name) {
            a11y::announce(*this, name);
        };

        closeButton_.setButtonText(LOC(kClose));
        closeButton_.setColour(juce::TextButton::buttonColourId, UIColors::accent);
        closeButton_.setColour(juce::TextButton::textColourOffId, UIColors::textPrimary);
        closeButton_.onClick = [this] { closeDialog(); };
        addAndMakeVisible(closeButton_);
    }

    ~TabbedPreferencesDialog() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(UIColors::backgroundDark);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(16);
        tabbedComponent_.setBounds(bounds.removeFromTop(bounds.getHeight() - 40));
        closeButton_.setBounds(bounds.removeFromRight(96).withTrimmedTop(6));
    }

    /** 非原生标题栏时移除 DialogWindow 默认标题栏，仅保留主题内容 */
    void parentHierarchyChanged() override
    {
        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
        {
            if (! dialogWindow->isUsingNativeTitleBar())
            {
                const int contentWidth = getWidth();
                const int contentHeight = getHeight();
                dialogWindow->setTitleBarHeight(0);
                dialogWindow->setContentComponentSize(contentWidth, contentHeight);
            }

            // Доступность: при открытии фокус был на самом окне, а не на элементе.
            // Ставим его на выбранную вкладку, когда окно уже на экране.
            juce::Component::SafePointer<TabbedPreferencesDialog> safeThis(this);
            juce::Timer::callAfterDelay(100, [safeThis] {
                if (safeThis == nullptr)
                    return;
                auto& bar = safeThis->tabbedComponent_.getTabbedButtonBar();
                if (auto* tabButton = bar.getTabButton(bar.getCurrentTabIndex()))
                    if (tabButton->isShowing())
                        tabButton->grabKeyboardFocus();
            });
        }
    }

    // Ctrl+Tab / Ctrl+Shift+Tab — следующая и предыдущая вкладка, как в окнах Windows.
    bool keyPressed(const juce::KeyPress& key) override
    {
        const auto mods = key.getModifiers();
        if (key.getKeyCode() == juce::KeyPress::tabKey && mods.isCtrlDown() && !mods.isAltDown()) {
            const int count = tabbedComponent_.getNumTabs();
            if (count > 0) {
                const int step = mods.isShiftDown() ? count - 1 : 1;
                tabbedComponent_.setCurrentTabIndex((tabbedComponent_.getCurrentTabIndex() + step) % count);
            }
            return true;
        }
        return false;
    }

private:
    // Кнопки вкладок: сообщаем имя открытой вкладки, иначе после Enter на кнопке тишина.
    class AnnouncingTabs final : public juce::TabbedComponent
    {
    public:
        AnnouncingTabs() : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop) {}
        std::function<void(const juce::String&)> onTabChanged;

        void currentTabChanged(int, const juce::String& newTabName) override
        {
            if (onTabChanged != nullptr && isShowing())
                onTabChanged(newTabName);
        }
    };

    // Страницы настроек устроены как «подпись, затем элемент». JUCE отдаёт скринридеру текст
    // подписи дважды (как имя и как значение), а у списков и ползунков имени нет вовсе.
    // Переносим текст подписи в имя элемента и скрываем саму подпись. У кнопок с собственным
    // текстом (сочетание клавиш, цвет) текст меняется, поэтому подпись идёт в описание.
    static void nameControlsByPrecedingLabels(juce::Component& page)
    {
        juce::Label* pendingLabel = nullptr;
        for (auto* child : page.getChildren()) {
            if (auto* label = dynamic_cast<juce::Label*>(child)) {
                pendingLabel = label;
                continue;
            }
            if (pendingLabel != nullptr && child->getTitle().isEmpty()) {
                const auto caption = pendingLabel->getText();
                auto* button = dynamic_cast<juce::Button*>(child);
                if (button != nullptr && button->getButtonText().isNotEmpty()) {
                    button->setDescription(caption);
                    pendingLabel->setAccessible(false);
                } else if (button != nullptr
                           || dynamic_cast<juce::ComboBox*>(child) != nullptr
                           || dynamic_cast<juce::Slider*>(child) != nullptr
                           || dynamic_cast<juce::TextEditor*>(child) != nullptr) {
                    child->setTitle(caption);
                    pendingLabel->setAccessible(false);
                }
            }
            pendingLabel = nullptr;
        }
    }

    void closeDialog()
    {
        if (closing_)
            return;

        closing_ = true;
        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState(0);
    }

    void applyCurrentLookAndFeel()
    {
        if (UIColors::currentThemeId() == ThemeId::Aurora) {
            setLookAndFeel(&auroraLookAndFeel_);
            return;
        }

        setLookAndFeel(&openTuneLookAndFeel_);
    }

    AnnouncingTabs tabbedComponent_;
    juce::TextButton closeButton_;
    OpenTuneLookAndFeel openTuneLookAndFeel_;
    AuroraLookAndFeel auroraLookAndFeel_;
    bool closing_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabbedPreferencesDialog)
};

} // namespace OpenTune
