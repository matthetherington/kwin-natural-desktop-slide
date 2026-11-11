// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "settling_swipe.h"
#include <QTest>

using namespace NaturalSwipe;
using namespace std::chrono_literals;

class SettlingSwipeTest : public QObject
{
    Q_OBJECT
    static SwipeFrame axis(SettlingSwipe &swipe, double delta, SwipeTracker::Time time,
                           bool valid = true, bool left = true, bool right = true)
    {
        return swipe.axis(delta, false, true, true, time, valid, left, right);
    }
private Q_SLOTS:
    void requiresSettlingWindow()
    {
        SettlingSwipe swipe;
        QVERIFY(!axis(swipe, 100, 0ms).consume);
        swipe.openWindow();
        QVERIFY(!axis(swipe, 0, 0ms).consume);
        QVERIFY(!axis(swipe, 100, 0ms, false).consume);
        swipe.closeWindow();
        QVERIFY(!axis(swipe, 100, 0ms).consume);
    }

    void continuousPreviewAndPartialReturn()
    {
        SettlingSwipe swipe;
        swipe.openWindow();
        auto frame = axis(swipe, 30, 100ms);
        QVERIFY(frame.consume && frame.preview);
        QCOMPARE(frame.offset, 0.1);
        QCOMPARE(frame.finish, SwipeFinish::None); // No eager one-shot switch.
        frame = axis(swipe, 30, 600ms);
        QCOMPARE(frame.offset, 0.2);
        QCOMPARE(axis(swipe, 0, 800ms).finish, SwipeFinish::Return);
        QVERIFY(!swipe.owned());
    }

    void dragBoundaryAndNaturalScroll()
    {
        for (int direction : {-1, 1}) {
            for (bool inverted : {false, true}) {
                for (double distance : {89.9, 90.0, 90.1}) {
                    SettlingSwipe swipe;
                    swipe.openWindow();
                    const double raw = direction * distance * (inverted ? -1 : 1);
                    swipe.axis(raw, inverted, true, true, 100ms, true, true, true);
                    const auto end = swipe.axis(0, inverted, true, true, 500ms, true, true, true);
                    const auto expected = distance < 90 ? SwipeFinish::Return
                        : direction > 0 ? SwipeFinish::Right : SwipeFinish::Left;
                    QCOMPARE(end.finish, expected);
                }
            }
        }
    }

    void repeatedFollowupsAfterSwitchesAndAborts()
    {
        SettlingSwipe swipe;
        // The adapter reopens only if its renderer is still settling after each
        // finish, including a return. There is no one-follow-up restriction.
        for (int direction : {1, 1, -1, -1, 1}) {
            swipe.openWindow();
            axis(swipe, direction * 30, 0ms);
            axis(swipe, direction * 30, 20ms);
            QCOMPARE(axis(swipe, 0, 40ms).finish,
                     direction > 0 ? SwipeFinish::Right : SwipeFinish::Left);
            swipe.openWindow();
            axis(swipe, direction * 60, 500ms);
            QCOMPARE(axis(swipe, 0, 1000ms).finish, SwipeFinish::Return);
        }
        swipe.closeWindow(); // Fully settled: ordinary scrolling resumes.
        QVERIFY(!axis(swipe, 100, 1100ms).consume);
    }

    void reversesBeforeRelease()
    {
        SettlingSwipe swipe;
        swipe.openWindow();
        axis(swipe, 180, 0ms);
        QCOMPARE(axis(swipe, -150, 300ms).offset, 0.1);
        QCOMPARE(axis(swipe, 0, 500ms).finish, SwipeFinish::Return);
        swipe.openWindow();
        axis(swipe, 180, 600ms);
        QCOMPARE(axis(swipe, -240, 640ms).offset, -0.2);
        QCOMPARE(axis(swipe, 0, 650ms).finish, SwipeFinish::Left);
    }

    void interruptionDrainsUntilStop()
    {
        SettlingSwipe swipe;
        swipe.openWindow();
        axis(swipe, 100, 0ms);
        QCOMPARE(swipe.interrupt().finish, SwipeFinish::Return);
        QVERIFY(swipe.draining());
        const auto tail = axis(swipe, -200, 10ms);
        QVERIFY(tail.consume && !tail.preview);
        const auto stop = axis(swipe, 0, 20ms);
        QVERIFY(stop.consume);
        QCOMPARE(stop.finish, SwipeFinish::None);
        QVERIFY(!axis(swipe, 200, 30ms).consume);
    }

    void verticalAndWheelAlwaysPass()
    {
        SettlingSwipe swipe;
        swipe.openWindow();
        QVERIFY(!swipe.axis(100, false, false, true, 0ms, true, true, true).consume);
        QVERIFY(!swipe.axis(100, false, true, false, 0ms, true, true, true).consume);
        axis(swipe, 60, 0ms);
        QVERIFY(!swipe.axis(0, false, false, true, 10ms, true, true, true).consume);
        QVERIFY(swipe.owned()); // Vertical stop cannot end a horizontal stream.
        axis(swipe, 0, 40ms);
        QVERIFY(!swipe.owned());
    }

    void boundaryClampReversalAndCancellation()
    {
        SettlingSwipe swipe;
        swipe.openWindow();
        const auto blocked = axis(swipe, -200, 0ms, true, false, true);
        QVERIFY(blocked.consume && !blocked.preview);
        const auto reverse = axis(swipe, 260, 40ms, true, false, true);
        QCOMPARE(reverse.offset, 0.2);
        QCOMPARE(axis(swipe, 0, 45ms).finish, SwipeFinish::Right);
        swipe.openWindow();
        axis(swipe, 100, 100ms);
        QCOMPARE(swipe.cancel(120ms).finish, SwipeFinish::Return);
        QVERIFY(!swipe.owned());
        QVERIFY(!swipe.windowOpen());
    }

    void startedStreamSurvivesWindowClosing()
    {
        SettlingSwipe swipe;
        swipe.openWindow();
        axis(swipe, 3, 0ms);
        swipe.closeWindow(); // Prior spring finished before acquisition.
        QVERIFY(axis(swipe, 57, 20ms).preview);
        QCOMPARE(axis(swipe, 0, 40ms).finish, SwipeFinish::Right);
    }

    void blockedThreeFingerSwipeAdmitsOppositeFollowup()
    {
        for (int edgeDirection : {-1, 1}) {
            SwipeSession initial;
            initial.begin(3, 0ms, true, edgeDirection > 0, edgeDirection < 0);
            initial.update({edgeDirection * 100.0, 0}, 100ms, true);
            QCOMPARE(initial.end(200ms, false, true).finish, SwipeFinish::None);
            QVERIFY(initial.boundaryRelease());

            SettlingSwipe followup;
            followup.openEdgeWindow(200ms);
            QVERIFY(followup.edgeWindow());
            axis(followup, -edgeDirection * 30, 450ms, true, edgeDirection > 0, edgeDirection < 0);
            QVERIFY(followup.owned());
            QVERIFY(!followup.edgeWindow());
            axis(followup, -edgeDirection * 30, 470ms, true, edgeDirection > 0, edgeDirection < 0);
            QCOMPARE(axis(followup, 0, 490ms).finish,
                     edgeDirection > 0 ? SwipeFinish::Left : SwipeFinish::Right);
        }
    }

    void edgeWindowExpiresWithoutExtendingOnScroll()
    {
        SettlingSwipe swipe;
        swipe.openEdgeWindow(200ms);
        QVERIFY(!swipe.windowOpen(199ms)); // Reject backwards timestamps.
        QVERIFY(swipe.windowOpen(599ms));
        QVERIFY(!swipe.windowOpen(600ms));
        QVERIFY(!axis(swipe, 100, 600ms).consume);
        QVERIFY(!axis(swipe, 100, 610ms).consume);
        QVERIFY(!swipe.owned());
        swipe.closeWindow();
        QVERIFY(!swipe.edgeWindow());
        swipe.openEdgeWindow(1000ms);
        QVERIFY(!swipe.axis(100, false, false, true, 1300ms, true, true, true).consume);
        QVERIFY(!axis(swipe, 100, 1400ms).consume); // Vertical scrolling did not renew grace.
    }

    void edgeAdmittedStreamCanOutliveGraceAndAbort()
    {
        SettlingSwipe swipe;
        swipe.openEdgeWindow(0ms);
        axis(swipe, -30, 390ms);
        QVERIFY(swipe.owned());
        axis(swipe, -30, 800ms);
        QCOMPARE(axis(swipe, 0, 1000ms).finish, SwipeFinish::Return);
        // The adapter may now reopen a real return-animation window, not a
        // renewed edge timeout. External interruption always closes either one.
        swipe.openWindow();
        QVERIFY(!swipe.edgeWindow());
        swipe.interrupt();
        QVERIFY(!axis(swipe, -100, 1100ms).consume);
    }
};

QTEST_GUILESS_MAIN(SettlingSwipeTest)
#include "settling_swipe_test.moc"
