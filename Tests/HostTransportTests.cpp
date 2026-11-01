#include "../Source/Utils/HostTransportCoalescer.h"
#include "../Source/Utils/PendingSeekPolicy.h"

#include <cstdio>
#include <vector>

using namespace OpenTune;

static int failures = 0;

static void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static bool isCall(const HostTransportCall& call, HostTransportCall::Kind kind, double seconds = 0.0)
{
    if (call.kind != kind)
        return false;
    if (kind == HostTransportCall::Kind::Seek)
        return call.seconds == seconds;
    return true;
}

static void testPendingSeekHoldsUntilHostMatches()
{
    PendingSeekPolicy::Input in;
    in.pendingTimeSeconds = 12.5;
    in.canonicalTimeSeconds = 1.0;
    in.issuedAtSeconds = 10.0;
    in.nowSeconds = 10.2;
    in.pendingEpoch = 4;
    in.currentEpoch = 9;
    in.hostBound = true;
    in.hostTimeAtGestureSeconds = 1.0;

    const auto held = PendingSeekPolicy::evaluate(in);
    check(held.holdPending && !held.clearPending,
          "a playing host that has not reached the seek stays on the pending time");

    in.canonicalTimeSeconds = 12.53;
    const auto matched = PendingSeekPolicy::evaluate(in);
    check(!matched.holdPending && matched.clearPending,
          "host time within tolerance after a new observation clears the pending seek");

    in.canonicalTimeSeconds = 12.5;
    in.currentEpoch = in.pendingEpoch;
    const auto sameEpoch = PendingSeekPolicy::evaluate(in);
    check(sameEpoch.holdPending && !sameEpoch.clearPending,
          "the observation captured before the request does not count as host confirmation");
}

static void testPendingSeekDeadline()
{
    PendingSeekPolicy::Input in;
    in.pendingTimeSeconds = 4.0;
    in.canonicalTimeSeconds = 0.0;
    in.issuedAtSeconds = 1.0;
    in.nowSeconds = 1.0 + PendingSeekPolicy::holdDeadlineSeconds - 0.01;
    in.pendingEpoch = 1;
    in.currentEpoch = 8;
    in.hostBound = true;
    in.hostTimeAtGestureSeconds = 0.0;

    const auto held = PendingSeekPolicy::evaluate(in);
    check(held.holdPending && !held.clearPending, "pending seek holds inside the deadline");

    in.nowSeconds = 1.0 + PendingSeekPolicy::holdDeadlineSeconds;
    in.hostBound = true;
    const auto expired = PendingSeekPolicy::evaluate(in);
    check(expired.holdPending && expired.detachFromHost && !expired.clearPending,
          "an ignored host seek stays on the edit cursor");

    in.pendingTimeSeconds = -1.0;
    const auto idle = PendingSeekPolicy::evaluate(in);
    check(!idle.holdPending && !idle.clearPending, "no pending seek is a no-op");
}

static void testLocalEditCursor()
{
    PendingSeekPolicy::Input in;
    in.pendingTimeSeconds = 8.0;
    in.canonicalTimeSeconds = 1.0;
    in.hostTimeAtGestureSeconds = 1.0;
    in.pendingEpoch = 2;
    in.currentEpoch = 6;
    in.hostBound = false;

    const auto held = PendingSeekPolicy::evaluate(in);
    check(held.holdPending && !held.clearPending,
          "a host that cannot seek keeps the in-plugin edit cursor");

    in.canonicalTimeSeconds = 8.02;
    const auto landed = PendingSeekPolicy::evaluate(in);
    check(landed.clearPending, "the edit cursor follows once the host lands on it");

    in.canonicalTimeSeconds = 3.5;
    in.hostTimeAtGestureSeconds = 1.0;
    const auto moved = PendingSeekPolicy::evaluate(in);
    check(moved.clearPending && !moved.holdPending,
          "moving the DAW playhead releases the local edit cursor");
}

static void testLogicRestartRule()
{
    check(logicCommitRestartsPlayback(true, true), "Logic restarts a committed seek while playing");
    check(!logicCommitRestartsPlayback(true, false), "Logic seeks in place while stopped");
    check(!logicCommitRestartsPlayback(false, true), "other hosts keep a plain seek while playing");
}

static void testCoalescedTimelineCommit()
{
    HostTransportCoalescer coalescer;
    coalescer.previewSeek(1.0);
    coalescer.previewSeek(2.0);
    coalescer.commitSeek(3.25, true);
    coalescer.previewSeek(4.5);

    const auto batch = coalescer.take();
    check(!coalescer.hasPending(), "take clears the coalescer");
    check(coalescer.take().empty(), "a second take is empty");

    const auto calls = planHostTransportCalls(batch);
    check(calls.size() == 3, "Logic commit is stop, seek, start");
    check(isCall(calls[0], HostTransportCall::Kind::Stop), "restart stops first");
    check(isCall(calls[1], HostTransportCall::Kind::Seek, 4.5), "latest seek wins");
    check(isCall(calls[2], HostTransportCall::Kind::Start), "restart resumes playback");
}

static void testPlainSeekRequests()
{
    HostTransportCoalescer preview;
    preview.previewSeek(8.0);
    const auto previewCalls = planHostTransportCalls(preview.take());
    check(previewCalls.size() == 1 && isCall(previewCalls[0], HostTransportCall::Kind::Seek, 8.0),
          "preview is a position request only");

    HostTransportCoalescer stopped;
    stopped.commitSeek(6.0, false);
    const auto stoppedCalls = planHostTransportCalls(stopped.take());
    check(stoppedCalls.size() == 1 && isCall(stoppedCalls[0], HostTransportCall::Kind::Seek, 6.0),
          "commit while stopped does not restart");
}

static void testHostStopClearsPendingSeek()
{
    PendingSeekPolicy::Input in;
    in.pendingTimeSeconds = 12.5;
    in.canonicalTimeSeconds = 1.0;
    in.hostTimeAtGestureSeconds = 1.0;
    in.issuedAtSeconds = 10.0;
    in.nowSeconds = 10.0 + PendingSeekPolicy::holdDeadlineSeconds + 0.01;
    in.pendingEpoch = 4;
    in.currentEpoch = 9;
    in.hostBound = true;
    in.hostWasPlayingAtGesture = true;
    in.hostIsPlayingNow = false;

    const auto decision = PendingSeekPolicy::evaluate(in);
    check(decision.clearPending && !decision.holdPending,
          "a host that stopped before reaching the seek drops the pending time");
}

static void testNearTargetBeatsHostStop()
{
    PendingSeekPolicy::Input in;
    in.pendingTimeSeconds = 12.5;
    in.canonicalTimeSeconds = 12.53;
    in.hostTimeAtGestureSeconds = 1.0;
    in.issuedAtSeconds = 10.0;
    in.nowSeconds = 10.0 + PendingSeekPolicy::holdDeadlineSeconds + 1.0;
    in.pendingEpoch = 4;
    in.currentEpoch = 9;
    in.hostBound = true;
    in.hostWasPlayingAtGesture = true;
    in.hostIsPlayingNow = false;

    const auto decision = PendingSeekPolicy::evaluate(in);
    check(decision.clearPending, "a host that landed on the seek clears it even after stopping");
}

static void testStoppedSeekHoldsPastDeadline()
{
    PendingSeekPolicy::Input in;
    in.pendingTimeSeconds = 12.5;
    in.canonicalTimeSeconds = 1.0;
    in.hostTimeAtGestureSeconds = 1.0;
    in.issuedAtSeconds = 10.0;
    in.nowSeconds = 10.0 + PendingSeekPolicy::holdDeadlineSeconds + 1.0;
    in.pendingEpoch = 4;
    in.currentEpoch = 9;
    in.hostBound = true;
    in.hostWasPlayingAtGesture = false;
    in.hostIsPlayingNow = false;

    const auto decision = PendingSeekPolicy::evaluate(in);
    check(decision.holdPending && decision.detachFromHost && !decision.clearPending,
          "a seek issued while stopped keeps the edit cursor past the deadline");
}

static void testPlayingSeekHoldsPastDeadline()
{
    PendingSeekPolicy::Input in;
    in.pendingTimeSeconds = 12.5;
    in.canonicalTimeSeconds = 1.0;
    in.hostTimeAtGestureSeconds = 1.0;
    in.issuedAtSeconds = 10.0;
    in.nowSeconds = 10.0 + PendingSeekPolicy::holdDeadlineSeconds + 1.0;
    in.pendingEpoch = 4;
    in.currentEpoch = 9;
    in.hostBound = true;
    in.hostWasPlayingAtGesture = true;
    in.hostIsPlayingNow = true;

    const auto decision = PendingSeekPolicy::evaluate(in);
    check(decision.holdPending && decision.detachFromHost && !decision.clearPending,
          "a still playing host keeps the edit cursor past the deadline");
}

int main()
{
    testPendingSeekHoldsUntilHostMatches();
    testPendingSeekDeadline();
    testLocalEditCursor();
    testLogicRestartRule();
    testCoalescedTimelineCommit();
    testPlainSeekRequests();
    testHostStopClearsPendingSeek();
    testNearTargetBeatsHostStop();
    testStoppedSeekHoldsPastDeadline();
    testPlayingSeekHoldsPastDeadline();

    if (failures == 0)
    {
        std::puts("All host transport tests passed.");
        return 0;
    }

    std::fprintf(stderr, "%d test(s) FAILED.\n", failures);
    return 1;
}
