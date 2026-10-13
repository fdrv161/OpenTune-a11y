#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Озвучка коротких сообщений для NVDA, JAWS и Экранного диктора (Windows, UI Automation)
// и VoiceOver (macOS).
namespace OpenTune::a11y {

enum class SpeechQueue
{
    replacePrevious, // новое сообщение прерывает прежнее: навигация, значения
    keepAll          // сообщение нельзя потерять: итог операции, ошибка
};

// Произнести текст от имени компонента source. Компонент должен быть на экране и в дереве
// доступности (обычно — элемент с фокусом). Title и description не меняются.
// Только поток сообщений.
void announce(juce::Component& source, const juce::String& text,
              SpeechQueue queue = SpeechQueue::replacePrevious);

} // namespace OpenTune::a11y
