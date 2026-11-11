// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "swipe_tracker.h"
#include <QTest>
#include <cmath>
#include <limits>

using namespace NaturalSwipe;
using namespace std::chrono_literals;
using State = SwipeTracker::State;

class SwipeTrackerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initialAndInactiveEvents()
    {
        SwipeTracker tracker;
        QCOMPARE(tracker.state(), State::Idle);
        tracker.update({100, 0}, 10ms);
        tracker.end(20ms);
        tracker.cancel();
        QCOMPARE(tracker.state(), State::Idle);
        QCOMPARE(tracker.progress(), 0.0);
    }

    void acceptsOnlyThreeFingers()
    {
        SwipeTracker tracker;
        for (int fingers : {0, 1, 2, 4, 5}) {
            tracker.begin(fingers, 0ms);
            tracker.update({200, 0}, 100ms);
            tracker.end(110ms);
            QCOMPARE(tracker.state(), State::Rejected);
            QCOMPARE(tracker.releaseDirection(), 0);
            QCOMPARE(tracker.progress(), 0.0);
        }
    }

    void angleTolerance_data()
    {
        QTest::addColumn<QPointF>("delta");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("right") << QPointF(20, 0) << true;
        QTest::newRow("left") << QPointF(-20, 0) << true;
        QTest::newRow("right-up-45") << QPointF(20, -20) << true;
        QTest::newRow("right-down-45") << QPointF(20, 20) << true;
        QTest::newRow("left-up-45") << QPointF(-20, -20) << true;
        QTest::newRow("left-down-45") << QPointF(-20, 20) << true;
        QTest::newRow("beyond-45") << QPointF(20, 20.01) << false;
        QTest::newRow("vertical") << QPointF(0, 20) << false;
    }

    void angleTolerance()
    {
        QFETCH(QPointF, delta);
        QFETCH(bool, accepted);
        SwipeTracker tracker;
        tracker.begin(3, 0ms);
        tracker.update(delta, 10ms);
        QCOMPARE(tracker.state(), accepted ? State::Horizontal : State::Rejected);
    }

    void thresholdAndAxisLock()
    {
        SwipeTracker tracker;
        tracker.begin(3, 0ms);
        tracker.update({3, 0}, 1ms);
        QCOMPARE(tracker.state(), State::Pending);
        QCOMPARE(tracker.progress(), 0.0);
        tracker.update({5, 0}, 2ms);
        QCOMPARE(tracker.state(), State::Horizontal);
        tracker.update({10, 100}, 3ms); // Curve after acquisition: do not reclassify.
        QCOMPARE(tracker.state(), State::Horizontal);
        QCOMPARE(tracker.progress(), 18.0 / SwipeTracker::distancePerDesktop);
    }

    void rejectionStaysRejected()
    {
        SwipeTracker tracker;
        tracker.begin(3, 0ms);
        tracker.update({0, 20}, 10ms);
        tracker.update({200, 0}, 20ms);
        QCOMPARE(tracker.state(), State::Rejected);
        QCOMPARE(tracker.progress(), 0.0);
    }

    void reversalAndPartialReturn()
    {
        SwipeTracker tracker;
        tracker.begin(3, 0ms);
        tracker.update({120, 0}, 10ms);
        QCOMPARE(tracker.progress(), 0.4);
        tracker.update({-90, 0}, 20ms);
        QCOMPARE(tracker.progress(), 0.1);
        tracker.end(21ms);
        QCOMPARE(tracker.state(), State::Finished);
        QCOMPARE(tracker.releaseDirection(), 0);
        tracker.update({300, 0}, 30ms);
        QCOMPARE(tracker.progress(), 0.1); // Ignore events after release.
    }

    void releaseDirectionsAndClamp()
    {
        SwipeTracker tracker;
        tracker.begin(3, 0ms);
        tracker.update({150, 0}, 100ms);
        tracker.end(101ms);
        QCOMPARE(tracker.releaseDirection(), 1);
        tracker.begin(3, 200ms);
        tracker.update({-1000, 0}, 300ms);
        QCOMPARE(tracker.progress(), -1.0);
        tracker.end(301ms);
        QCOMPARE(tracker.releaseDirection(), -1);
        tracker.begin(3, 400ms);
        tracker.update({100, 0}, 410ms);
        tracker.update({-250, 0}, 420ms); // Reverse through the origin.
        tracker.end(421ms);
        QCOMPARE(tracker.releaseDirection(), -1);
    }

    void slowDistanceReleaseAtThirtyPercent()
    {
        for (int sign : {-1, 1}) {
            for (double distance : {89.9, 90.0, 90.1}) {
                SwipeTracker tracker;
                tracker.begin(3, 0ms);
                tracker.update({sign * distance, 0}, 2000ms);
                tracker.end(2200ms);
                QCOMPARE(tracker.velocity(), 0.0);
                QCOMPARE(tracker.releaseDirection(), distance < 90.0 ? 0 : sign);
            }
        }
    }

    void cancellationRetainsFiniteReturnPosition()
    {
        SwipeTracker tracker;
        tracker.begin(3, 0ms);
        tracker.update({200, 0}, 100ms);
        tracker.cancel();
        QCOMPARE(tracker.state(), State::Cancelled);
        QCOMPARE(tracker.progress(), 200.0 / 300.0);
        QCOMPARE(tracker.velocity(), 0.0);
        QCOMPARE(tracker.releaseDirection(), 0);
        tracker.end(110ms);
        QCOMPARE(tracker.state(), State::Cancelled);
    }

    void recentVelocityAndPause()
    {
        SwipeTracker tracker;
        tracker.begin(3, 0ms);
        tracker.update({30, 0}, 10ms);
        tracker.update({30, 0}, 20ms);
        QCOMPARE(tracker.velocity(), 10.0);
        tracker.end(250ms);
        QCOMPARE(tracker.velocity(), 0.0);
        QCOMPARE(tracker.releaseDirection(), 0);
        tracker.begin(3, 300ms);
        tracker.update({30, 0}, 301ms);
        tracker.end(302ms);
        QVERIFY(tracker.velocity() > 0);
        QCOMPARE(tracker.releaseDirection(), 0); // Velocity does not commit yet.
    }

    void releaseNearVelocityWindowBoundary()
    {
        SwipeTracker tracker;
        tracker.begin(3, 0ms);
        tracker.update({150, 0}, 100ms);
        tracker.end(101ms);
        QVERIFY(std::abs(tracker.velocity() - 4.95) < 1e-9);
    }

    void flickCompletionGuards()
    {
        SwipeTracker tracker;
        for (int sign : {-1, 1}) {
            tracker.begin(3, 0ms);
            tracker.update({sign * 60.0, 0}, 40ms);
            tracker.end(45ms, true);
            QCOMPARE(tracker.releaseDirection(), sign);

            tracker.begin(3, 0ms);
            tracker.update({sign * 60.0, 0}, 40ms);
            tracker.end(250ms, true); // Pause eliminates momentum.
            QCOMPARE(tracker.releaseDirection(), 0);

            tracker.begin(3, 0ms);
            tracker.update({sign * 3.0, 0}, 1ms);
            tracker.end(2ms, true); // Below horizontal acquisition threshold.
            QCOMPARE(tracker.releaseDirection(), 0);

            tracker.begin(3, 0ms);
            tracker.update({sign * 120.0, 0}, 100ms);
            tracker.update({-sign * 60.0, 0}, 140ms);
            tracker.end(145ms, true); // Reversing toward origin: never flick opposite.
            QCOMPARE(tracker.releaseDirection(), 0);
        }
    }

    void shortLiveFlickRegression()
    {
        // Failed live opposite-direction follow-up: 10.4% displacement in
        // 46.905 ms. Momentum projects past 50%, but the old 15% floor vetoed it.
        for (int sign : {-1, 1}) {
            SwipeTracker tracker;
            tracker.begin(3, 0us);
            tracker.update({sign * 0.1043559886317499 * SwipeTracker::distancePerDesktop, 0}, 46905us);
            tracker.end(46905us, true);
            QCOMPARE(tracker.releaseDirection(), sign);

            tracker.begin(3, 0us);
            tracker.update({sign * 0.1043559886317499 * SwipeTracker::distancePerDesktop, 0}, 46905us);
            tracker.end(250ms, true);
            QCOMPARE(tracker.releaseDirection(), 0); // Same distance, pause before lift.

            tracker.begin(3, 0us);
            tracker.update({sign * 3.0, 0}, 1ms);
            tracker.end(2ms, true);
            QCOMPARE(tracker.releaseDirection(), 0); // Pending motion never commits.
        }
    }

    void shorterLiveFlickRegression()
    {
        // Second labeled failure: the lowered 8% floor still vetoed this
        // 7.63% flick, whose momentum projects to just over 50%.
        for (int sign : {-1, 1}) {
            SwipeTracker tracker;
            tracker.begin(3, 0us);
            tracker.update({sign * 0.0762786845310597 * SwipeTracker::distancePerDesktop, 0}, 31799us);
            tracker.end(31799us, true);
            QCOMPARE(tracker.releaseDirection(), sign);

            tracker.begin(3, 0us);
            tracker.update({sign * 0.0762786845310597 * SwipeTracker::distancePerDesktop, 0}, 500ms);
            tracker.end(501ms, true);
            QCOMPARE(tracker.releaseDirection(), 0); // Same short distance, slow swipe.

            tracker.begin(3, 0us);
            tracker.update({0, sign * 30.0}, 1ms);
            tracker.end(2ms, true);
            QCOMPARE(tracker.releaseDirection(), 0); // Vertical motion never flicks.
        }
    }

    void flickSpeedIsAlternativeToDragDistance()
    {
        for (int sign : {-1, 1}) {
            SwipeTracker tracker;
            // Third failed live sample: the 180 ms projection only reached
            // 37.4%, despite an acquired, same-direction fast flick.
            tracker.begin(3, 0us);
            tracker.update({sign * 0.07844774665042631 * SwipeTracker::distancePerDesktop, 0}, 47725us);
            tracker.end(47725us, true);
            QCOMPARE(tracker.releaseDirection(), sign);

            tracker.begin(3, 0us);
            tracker.update({sign * 15.0, 0}, 40ms); // 5%, 1.25 desktops/second.
            tracker.end(40ms, true);
            QCOMPARE(tracker.releaseDirection(), sign);

            tracker.begin(3, 0us);
            tracker.update({sign * 15.0, 0}, 60ms); // 5%, below flick speed.
            tracker.end(60ms, true);
            QCOMPARE(tracker.releaseDirection(), 0);
        }
    }

    void equalTimestampsAndFreshBegin()
    {
        SwipeTracker tracker;
        tracker.begin(3, 0ms);
        tracker.update({10, 0}, 0ms);
        tracker.update({10, 0}, 0ms);
        QVERIFY(std::isfinite(tracker.velocity()));
        tracker.begin(3, 10ms);
        QCOMPARE(tracker.state(), State::Pending);
        QCOMPARE(tracker.displacement(), QPointF());
        QCOMPARE(tracker.velocity(), 0.0);
        tracker.reset();
        QCOMPARE(tracker.state(), State::Idle);
    }

    void invalidDataFailsClosed()
    {
        SwipeTracker tracker;
        tracker.begin(3, -1ms);
        QCOMPARE(tracker.state(), State::Rejected);
        tracker.begin(3, 10ms);
        tracker.update({10, 0}, 9ms);
        QCOMPARE(tracker.state(), State::Cancelled);
        tracker.begin(3, 10ms);
        tracker.end(9ms);
        QCOMPARE(tracker.state(), State::Cancelled);
        tracker.begin(3, 0ms);
        tracker.update({std::numeric_limits<double>::quiet_NaN(), 0}, 1ms);
        QCOMPARE(tracker.state(), State::Cancelled);
        QVERIFY(std::isfinite(tracker.progress()));
        tracker.begin(3, 0ms);
        tracker.update({std::numeric_limits<double>::infinity(), 0}, 1ms);
        QCOMPARE(tracker.state(), State::Cancelled);
        tracker.begin(3, 0ms);
        tracker.update({std::numeric_limits<double>::max(), 0}, 1us);
        QCOMPARE(tracker.state(), State::Cancelled);
        QVERIFY(std::isfinite(tracker.velocity()));
    }

    void manySamplesStayFinite()
    {
        SwipeTracker tracker;
        tracker.begin(3, 0us);
        for (int i = 1; i <= 10000; ++i) tracker.update({0.1, 0}, std::chrono::microseconds(i));
        QCOMPARE(tracker.state(), State::Horizontal);
        QVERIFY(std::isfinite(tracker.velocity()));
        QCOMPARE(tracker.progress(), 1.0);
    }
};

QTEST_GUILESS_MAIN(SwipeTrackerTest)
#include "swipe_tracker_test.moc"
