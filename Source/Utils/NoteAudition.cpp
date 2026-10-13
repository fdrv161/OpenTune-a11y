#include "NoteAudition.h"

namespace OpenTune {

void NoteAudition::play(const juce::AudioBuffer<float>& source, double sourceSampleRate,
                        int startSample, int numSamples)
{
    const int sourceLength = source.getNumSamples();
    const int sourceChannels = source.getNumChannels();
    if (sourceSampleRate <= 0.0 || sourceChannels <= 0 || sourceLength <= 0)
        return;

    startSample = juce::jlimit(0, sourceLength, startSample);
    numSamples = juce::jlimit(0, sourceLength - startSample, numSamples);
    numSamples = juce::jmin(numSamples, static_cast<int>(kMaxDurationSeconds * sourceSampleRate));
    if (numSamples <= 0)
        return;

    // Готовим кусок заранее: моно и с плавными краями (5 мс), чтобы не было щелчков.
    auto clip = std::make_unique<Clip>();
    clip->sampleRate = sourceSampleRate;
    clip->mono.setSize(1, numSamples);
    clip->mono.clear();
    const float channelGain = 1.0f / static_cast<float>(sourceChannels);
    for (int ch = 0; ch < sourceChannels; ++ch)
        clip->mono.addFrom(0, 0, source, ch, startSample, numSamples, channelGain);

    const int fade = juce::jmin(numSamples / 2, static_cast<int>(0.005 * sourceSampleRate));
    if (fade > 0) {
        clip->mono.applyGainRamp(0, 0, fade, 0.0f, 1.0f);
        clip->mono.applyGainRamp(0, numSamples - fade, fade, 1.0f, 0.0f);
    }

    std::unique_ptr<Clip> toFree1, toFree2;
    {
        const juce::SpinLock::ScopedLockType lock(lock_);
        toFree1 = std::move(retired_);
        toFree2 = std::move(pending_);
        pending_ = std::move(clip);
        stopRequested_ = false;
    }
    // toFree1/toFree2 освобождаются здесь, в потоке сообщений.
}

void NoteAudition::stop()
{
    std::unique_ptr<Clip> toFree1, toFree2;
    {
        const juce::SpinLock::ScopedLockType lock(lock_);
        toFree1 = std::move(retired_);
        toFree2 = std::move(pending_);
        stopRequested_ = true;
    }
}

void NoteAudition::takePendingClip()
{
    // Не ждём блокировку: если поток сообщений сейчас её держит, заберём кусок в следующем блоке.
    const juce::SpinLock::ScopedTryLockType tryLock(lock_);
    if (!tryLock.isLocked())
        return;

    if (pending_ == nullptr && !stopRequested_)
        return;

    // Старый кусок можно отложить, только если место для отложенного свободно.
    // Иначе подождём, пока поток сообщений его освободит (тогда кусок доиграет ещё блок).
    if (playing_ != nullptr && retired_ != nullptr)
        return;

    if (playing_ != nullptr)
        retired_ = std::move(playing_);

    if (stopRequested_) {
        stopRequested_ = false;
        return;
    }

    playing_ = std::move(pending_);
    position_ = 0.0;
}

void NoteAudition::mixIntoBuffer(juce::AudioBuffer<float>& output, int numSamples, double outputSampleRate)
{
    takePendingClip();

    if (playing_ == nullptr || outputSampleRate <= 0.0)
        return;

    const auto& clip = *playing_;
    const int clipLength = clip.mono.getNumSamples();
    if (position_ >= static_cast<double>(clipLength - 1))
        return; // доиграли — остаёмся тихими до следующей ноты

    const float* src = clip.mono.getReadPointer(0);
    const double step = clip.sampleRate / outputSampleRate;
    const int outChannels = output.getNumChannels();
    numSamples = juce::jmin(numSamples, output.getNumSamples());

    for (int i = 0; i < numSamples; ++i) {
        const int index = static_cast<int>(position_);
        if (index >= clipLength - 1)
            break;
        // Линейная интерполяция: для прослушивания достаточно, если частоты не совпадают.
        const float frac = static_cast<float>(position_ - static_cast<double>(index));
        const float sample = src[index] + (src[index + 1] - src[index]) * frac;
        for (int ch = 0; ch < outChannels; ++ch)
            output.addSample(ch, i, sample);
        position_ += step;
    }
}

} // namespace OpenTune
