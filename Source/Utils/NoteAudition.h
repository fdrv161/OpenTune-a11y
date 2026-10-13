#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <memory>

namespace OpenTune {

/**
 * Прослушивание спетой ноты: при переходе стрелками по нотам играет кусок исходного
 * вокала этой ноты, как в Melodyne. Транспорт DAW при этом не трогается — звук
 * подмешивается в выход плагина так же, как пробник клавиш фортепиано.
 *
 * play/stop вызываются из потока сообщений, mixIntoBuffer — из аудиопотока.
 * Аудиопоток не выделяет и не освобождает память: старый кусок он откладывает
 * в retired_, а освобождает его поток сообщений при следующем вызове.
 */
class NoteAudition {
public:
    NoteAudition() = default;

    /// Сыграть кусок source с startSample длиной numSamples (заменяет то, что звучит сейчас).
    void play(const juce::AudioBuffer<float>& source, double sourceSampleRate,
              int startSample, int numSamples);
    void stop();

    void mixIntoBuffer(juce::AudioBuffer<float>& output, int numSamples, double outputSampleRate);

    static constexpr double kMaxDurationSeconds = 4.0;

private:
    struct Clip {
        juce::AudioBuffer<float> mono;
        double sampleRate = 44100.0;
    };

    void takePendingClip();

    juce::SpinLock lock_;
    std::unique_ptr<Clip> pending_;   // под lock_
    std::unique_ptr<Clip> retired_;   // под lock_
    bool stopRequested_ = false;      // под lock_

    // Только аудиопоток.
    std::unique_ptr<Clip> playing_;
    double position_ = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteAudition)
};

} // namespace OpenTune
