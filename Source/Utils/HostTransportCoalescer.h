#pragma once

#include <vector>

namespace OpenTune {

// One flush of host transport requests. Later commands overwrite earlier ones
// so a timeline drag collapses to the position under the cursor.
struct HostTransportBatch
{
    bool hasSeek = false;
    double seekSeconds = 0.0;
    bool seekIsCommit = false;
    bool restartIfPlaying = false;

    bool empty() const noexcept
    {
        return !hasSeek;
    }
};

// Logic ignores requestSetPlaybackPosition while the transport is running.
// A committed seek on that host has to stop, move, then start again.
inline bool logicCommitRestartsPlayback(bool hostIsLogic, bool hostIsPlaying) noexcept
{
    return hostIsLogic && hostIsPlaying;
}

class HostTransportCoalescer
{
public:
    void previewSeek(double seconds) noexcept
    {
        batch_.hasSeek = true;
        batch_.seekSeconds = seconds;
    }

    void commitSeek(double seconds, bool restartIfPlaying) noexcept
    {
        batch_.hasSeek = true;
        batch_.seekSeconds = seconds;
        batch_.seekIsCommit = true;
        batch_.restartIfPlaying = restartIfPlaying;
    }

    bool hasPending() const noexcept { return !batch_.empty(); }

    HostTransportBatch take() noexcept
    {
        const HostTransportBatch out = batch_;
        batch_ = {};
        return out;
    }

private:
    HostTransportBatch batch_{};
};

struct HostTransportCall
{
    enum class Kind
    {
        Stop,
        Seek,
        Start
    };

    Kind kind = Kind::Stop;
    double seconds = 0.0;
};

// Host call order. Logic applies a seek only after playback has stopped,
// so a restart issues stop, position, then start.
inline std::vector<HostTransportCall> planHostTransportCalls(const HostTransportBatch& batch)
{
    std::vector<HostTransportCall> calls;
    if (!batch.hasSeek)
        return calls;

    if (batch.seekIsCommit && batch.restartIfPlaying)
    {
        calls.reserve(3);
        calls.push_back({ HostTransportCall::Kind::Stop, 0.0 });
        calls.push_back({ HostTransportCall::Kind::Seek, batch.seekSeconds });
        calls.push_back({ HostTransportCall::Kind::Start, 0.0 });
    }
    else
    {
        calls.reserve(1);
        calls.push_back({ HostTransportCall::Kind::Seek, batch.seekSeconds });
    }
    return calls;
}

} // namespace OpenTune
