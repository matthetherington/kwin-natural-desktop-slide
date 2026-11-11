// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "swipe_session.h"
#include <QTest>

using namespace NaturalSwipe;
using namespace std::chrono_literals;

class SwipeSessionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void declinedStreamsPassThrough()
    {
        SwipeSession session;
        QVERIFY(!session.begin(4, 0ms, true, true, true).consume);
        QVERIFY(!session.update({200, 0}, 20ms, true).consume);
        QVERIFY(!session.end(21ms, false, true).consume);
        QVERIFY(!session.begin(3, 0ms, false, true, true).consume);
    }

    void partialPreviewReturns()
    {
        SwipeSession session;
        QVERIFY(session.begin(3, 0ms, true, true, true).consume);
        const auto frame = session.update({60, 0}, 500ms, true);
        QVERIFY(frame.consume && frame.preview);
        QCOMPARE(frame.offset, 60.0 / 300.0);
        QCOMPARE(session.end(501ms, false, true).finish, SwipeFinish::Return);
        QVERIFY(!session.owned());
    }

    void slowDistanceBasedCompletionAndReversal()
    {
        SwipeSession session;
        session.begin(3, 0ms, true, true, true);
        for (int i = 1; i <= 20; ++i) session.update({-10, 0}, std::chrono::milliseconds(i * 100), true);
        QCOMPARE(session.end(2500ms, false, true).finish, SwipeFinish::Left);
        session.begin(3, 3000ms, true, true, true);
        session.update({200, 0}, 3100ms, true);
        QCOMPARE(session.end(3200ms, false, true).finish, SwipeFinish::Right);
        session.begin(3, 4000ms, true, true, true);
        session.update({200, 0}, 4100ms, true);
        session.update({-150, 0}, 4200ms, true);
        QCOMPARE(session.end(4300ms, false, true).finish, SwipeFinish::Return);
    }

    void slowDragThirtyPercentBoundary()
    {
        for (int direction : {-1, 1}) {
            for (double distance : {89.9, 90.0, 90.1}) {
                SwipeSession session;
                session.begin(3, 0ms, true, true, true);
                session.update({direction * distance, 0}, 2000ms, true);
                const auto expected = distance < 90.0 ? SwipeFinish::Return
                    : direction > 0 ? SwipeFinish::Right : SwipeFinish::Left;
                QCOMPARE(session.end(2200ms, false, true).finish, expected);
            }
        }
    }

    void identifiesBlockedBoundaryReleaseOnly()
    {
        for (int direction : {-1, 1}) {
            SwipeSession session;
            session.begin(3, 0ms, true, direction > 0, direction < 0);
            QVERIFY(!session.update({direction * 100.0, 0}, 100ms, true).preview);
            QCOMPARE(session.end(120ms, false, true).finish, SwipeFinish::None);
            QVERIFY(session.boundaryRelease());
            session.begin(3, 200ms, true, true, true);
            QVERIFY(!session.boundaryRelease());
            session.update({0, 100}, 250ms, true);
            session.end(300ms, false, true);
            QVERIFY(!session.boundaryRelease());
            session.begin(3, 400ms, true, direction > 0, direction < 0);
            session.update({direction * 100.0, 0}, 500ms, true);
            session.end(520ms, true, true);
            QVERIFY(!session.boundaryRelease()); // Input cancellation is not an edge release.
        }
    }

    void ownsRejectedVerticalStreamUntilEnd()
    {
        SwipeSession session;
        QVERIFY(session.begin(3, 0ms, true, true, true).consume);
        const auto frame = session.update({0, 100}, 10ms, true);
        QVERIFY(frame.consume && !frame.preview);
        const auto end = session.end(20ms, false, true);
        QVERIFY(end.consume);
        QCOMPARE(end.finish, SwipeFinish::None);
    }

    void interruptedStreamsDrainWithoutCommit()
    {
        SwipeSession session;
        session.begin(3, 0ms, true, true, true);
        session.update({200, 0}, 20ms, true);
        QCOMPARE(session.update({10, 0}, 30ms, false).finish, SwipeFinish::Return);
        QVERIFY(session.draining());
        const auto tail = session.update({200, 0}, 40ms, true);
        QVERIFY(tail.consume && !tail.preview);
        QCOMPARE(session.end(50ms, false, true).finish, SwipeFinish::None);
        QVERIFY(!session.owned());
    }

    void explicitCancellationAndBadTimestamp()
    {
        SwipeSession session;
        session.begin(3, 0ms, true, true, true);
        session.update({200, 0}, 20ms, true);
        QCOMPARE(session.end(30ms, true, true).finish, SwipeFinish::Return);
        session.begin(3, 100ms, true, true, true);
        session.update({200, 0}, 120ms, true);
        QCOMPARE(session.end(110ms, false, true).finish, SwipeFinish::Return);
    }

    void boundariesClampAndAllowReversal()
    {
        SwipeSession session;
        session.begin(3, 0ms, true, false, true);
        const auto blocked = session.update({-200, 0}, 20ms, true);
        QVERIFY(blocked.consume && !blocked.preview);
        QCOMPARE(blocked.offset, 0.0);
        const auto reverse = session.update({400, 0}, 40ms, true);
        QVERIFY(reverse.preview);
        QCOMPARE(reverse.offset, 200.0 / 300.0);
        QCOMPARE(session.end(50ms, false, true).finish, SwipeFinish::Right);
    }

    void unexpectedBeginCancelsThenDrains()
    {
        SwipeSession session;
        session.begin(3, 0ms, true, true, true);
        session.update({200, 0}, 20ms, true);
        const auto restart = session.begin(4, 30ms, true, true, true);
        QVERIFY(restart.consume);
        QCOMPARE(restart.finish, SwipeFinish::Return);
        QVERIFY(session.draining());
        QCOMPARE(session.end(40ms, false, true).finish, SwipeFinish::None);
    }

    void shortFlicksCompleteInPhysicalDirection()
    {
        for (int direction : {-1, 1}) {
            SwipeSession session;
            session.begin(3, 0ms, true, true, true);
            session.update({direction * 60.0, 0}, 40ms, true);
            QCOMPARE(session.end(45ms, false, true).finish,
                     direction > 0 ? SwipeFinish::Right : SwipeFinish::Left);
        }
    }

    void settlingStreamsStayConsumedEvenIfAnimationFinishes()
    {
        SwipeSession session;
        for (int i = 0; i < 5; ++i) {
            QVERIFY(session.begin(3, 0ms, true, true, true, false).consume);
            QVERIFY(session.draining());
            const auto frame = session.update({200, 0}, 20ms, true);
            QVERIFY(frame.consume && !frame.preview);
            const auto end = session.end(25ms, false, true);
            QVERIFY(end.consume);
            QCOMPARE(end.finish, SwipeFinish::None);
        }
        // A fresh stream after settling can switch normally.
        session.begin(3, 100ms, true, true, true);
        session.update({60, 0}, 140ms, true);
        QCOMPARE(session.end(145ms, false, true).finish, SwipeFinish::Right);
    }

    void noNeighboursStillOwnsStream()
    {
        SwipeSession session;
        QVERIFY(session.begin(3, 0ms, true, false, false).consume);
        QVERIFY(session.draining());
        QVERIFY(!session.update({200, 0}, 20ms, true).preview);
        QCOMPARE(session.end(25ms, false, true).finish, SwipeFinish::None);
    }

    void nearCompleteThenFlickCommitsTwoDesktops()
    {
        for (int direction : {-1, 1}) {
            SwipeSession session;
            const auto expected = direction > 0 ? SwipeFinish::Right : SwipeFinish::Left;
            session.begin(3, 0ms, true, true, true);
            const auto first = session.update({direction * 297.0, 0}, 1000ms, true);
            QCOMPARE(first.offset, direction * 0.99);
            QCOMPARE(session.end(1010ms, false, true).finish, expected);
            // The adapter may permit a new stream while its own renderer is
            // still settling. The model starts afresh at the committed desktop.
            QVERIFY(session.begin(3, 1020ms, true, true, true).consume);
            QVERIFY(!session.draining());
            const auto next = session.update({direction * 60.0, 0}, 1060ms, true);
            QCOMPARE(next.offset, direction * 0.2);
            QCOMPARE(session.end(1065ms, false, true).finish, expected);
        }
    }

    void repeatedAcceptedFlicksAndDirectionChanges()
    {
        SwipeSession session;
        for (int direction : {1, 1, 1, -1, -1}) {
            session.begin(3, 0ms, true, true, true);
            session.update({direction * 60.0, 0}, 40ms, true);
            QCOMPARE(session.end(45ms, false, true).finish,
                     direction > 0 ? SwipeFinish::Right : SwipeFinish::Left);
        }
        // A follow-up partial gesture still returns, not a stale prior commit.
        session.begin(3, 0ms, true, true, true);
        session.update({60, 0}, 500ms, true);
        QCOMPARE(session.end(510ms, false, true).finish, SwipeFinish::Return);
    }

    void slowCommitThenShortOppositeFlick()
    {
        for (int direction : {-1, 1}) {
            SwipeSession session;
            session.begin(3, 0ms, true, true, true);
            session.update({direction * 180.0, 0}, 1000ms, true);
            QCOMPARE(session.end(1010ms, false, true).finish,
                     direction > 0 ? SwipeFinish::Right : SwipeFinish::Left);
            session.begin(3, 1020ms, true, true, true);
            session.update({-direction * 0.07844774665042631 * SwipeTracker::distancePerDesktop, 0}, 1067725us, true);
            QCOMPARE(session.end(1067725us, false, true).finish,
                     direction > 0 ? SwipeFinish::Left : SwipeFinish::Right);
        }
    }
};

QTEST_GUILESS_MAIN(SwipeSessionTest)
#include "swipe_session_test.moc"
