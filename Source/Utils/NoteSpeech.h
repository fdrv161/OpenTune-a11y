#pragma once

// Текст о высоте ноты для скринридера: «A4 +25 ct», «C sharp 4».
// Только стандартная библиотека, чтобы формат можно было проверить тестом без JUCE.

#include <cmath>
#include <string>

namespace OpenTune::NoteSpeech {

// Ближайшая нота и отклонение от неё в центах (от -50 до +49).
struct PitchParts
{
    int midi = 0;
    int cents = 0;
};

inline PitchParts splitMidi(float midi)
{
    // Сначала округляем до цента, потом делим: иначе 68.996 дало бы «A4 -0 ct».
    const long totalCents = std::lround(static_cast<double>(midi) * 100.0);
    const long nearest = (totalCents + 50) / 100;
    return { static_cast<int>(nearest), static_cast<int>(totalCents - nearest * 100) };
}

// «A4» или «C sharp 4». Символ # не используем: синтезаторы речи читают его по-разному
// («решётка», «номер»), поэтому диез передаётся словом sharpWord.
inline std::string noteName(int midi, const std::string& sharpWord)
{
    static const char* const letters[12] = { "C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B" };
    static const bool sharp[12] = { false, true, false, true, false, false, true, false, true, false, true, false };

    const int pitchClass = ((midi % 12) + 12) % 12;
    // Целочисленное деление «вниз» для отрицательных номеров, чтобы октава не сбивалась.
    const int octave = (midi >= 0 ? midi / 12 : (midi - 11) / 12) - 1;

    std::string text = letters[pitchClass];
    if (sharp[pitchClass])
        text += " " + sharpWord + " ";
    text += std::to_string(octave);
    return text;
}

// «A4 +25 ct»; при нулевом отклонении — просто «A4».
inline std::string describeMidi(float midi, const std::string& sharpWord)
{
    const auto parts = splitMidi(midi);
    std::string text = noteName(parts.midi, sharpWord);
    if (parts.cents != 0)
        text += (parts.cents > 0 ? " +" : " ") + std::to_string(parts.cents) + " ct";
    return text;
}

} // namespace OpenTune::NoteSpeech
