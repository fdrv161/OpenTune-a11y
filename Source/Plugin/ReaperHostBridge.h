#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

namespace OpenTune {

// REAPER gives VST3 plugins its own API (IReaperHostApplication). OpenTune uses it for one
// thing: the edit cursor. While the transport is stopped REAPER does not report it to the
// plugin, so keyboard paste landed where playback last stopped instead of at the cursor the
// user (and the screen reader) sees. In other hosts nothing is found and nothing changes.
class ReaperHostBridge final : public juce::VST3ClientExtensions
{
public:
    void setIHostApplication(Steinberg::FUnknown* host) override;

    // Edit cursor in project seconds, only while REAPER is stopped or paused;
    // empty in other hosts or while playing (then the host playhead is current).
    std::optional<double> getStoppedEditCursorSeconds() const;

private:
    double (*getCursorPosition_)() = nullptr;
    int (*getPlayState_)() = nullptr;
};

} // namespace OpenTune
