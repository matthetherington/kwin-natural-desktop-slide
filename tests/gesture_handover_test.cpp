// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "gesture_handover.h"
#include "swipe_session.h"
#include "settling_swipe.h"
#include <QTest>
#include <cmath>

using namespace NaturalSwipe;
using namespace std::chrono_literals;

class GestureHandoverTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void freshGestureUsesLogicalOriginAndCumulativeOffsets()
    {
        GestureHandover handover;
        QCOMPARE(handover.update({2, 1}, {0.1, 0}), QPointF(2.1, 1));
        QCOMPARE(handover.update({2, 1}, {0.2, 0}), QPointF(2.2, 1));
        QCOMPARE(handover.update({2, 1}, {0, 0}), QPointF(2, 1));
    }

    void interruptCommit_data()
    {
        QTest::addColumn<double>("direction");
        QTest::newRow("forward") << 1.0;
        QTest::newRow("reverse") << -1.0;
    }

    void interruptCommit()
    {
        QFETCH(double, direction);
        GestureHandover handover;
        // Desktop 2 is selected, but the image is still 30% short of it.
        const QPointF selected(1, 0), visible(0.7, 0);
        const QPointF offset(direction * 0.03, 0);
        QCOMPARE(handover.update(selected, offset, visible), visible + offset);
        // The inherited origin must be captured exactly once, not repeatedly
        // added or recomputed from the selected desktop on later updates.
        QCOMPARE(handover.update(selected, offset * 2), visible + offset * 2);
        QCOMPARE(handover.update(selected, {}), visible);
    }

    void interruptPartialReturnAndEdgeBounce()
    {
        for (double visible : {-0.08, 0.08, 0.35, -0.35}) {
            GestureHandover handover;
            QCOMPARE(handover.update({}, {}, QPointF(visible, 0)), QPointF(visible, 0));
            QCOMPARE(handover.update({}, {0.02, 0}), QPointF(visible + 0.02, 0));
            QCOMPARE(handover.update({}, {-0.02, 0}), QPointF(visible - 0.02, 0));
        }
    }

    void consecutiveGesturesResetTheOrigin()
    {
        GestureHandover handover;
        handover.update({1, 0}, {0.1, 0}, QPointF(0.7, 0));
        handover.reset(); // Release: the renderer starts its next spring.
        QCOMPARE(handover.update({2, 0}, {}, QPointF(1.4, 0)), QPointF(1.4, 0));
        handover.reset(); // Return/finish/topology change/unload.
        QCOMPARE(handover.update({2, 0}, {0.1, 0}), QPointF(2.1, 0));
    }

    void wrappingKeepsTheVisualCoordinateBranch()
    {
        GestureHandover handover;
        // Three-desktop row: logically 0, visually approaching 3 from the left.
        const auto position = handover.update({}, {0.08, 0}, QPointF(2.95, 0));
        QVERIFY(std::abs(position.x() - 3.03) < 1e-10);
        QVERIFY(std::abs(std::fmod(position.x(), 3.0) - 0.03) < 1e-10);
    }

    void inheritedDistanceDoesNotChangeReleasePolicy()
    {
        GestureHandover handover;
        SwipeSession swipe;
        swipe.begin(3, 0ms, true, true, true);
        const auto frame = swipe.update({30, 0}, 1000ms, true);
        const auto position = handover.update({}, {frame.offset, 0}, QPointF(0.7, 0));
        QVERIFY(position.x() > 0.3); // Image displacement is not input progress.
        QCOMPARE(swipe.end(1200ms, false, true).finish, SwipeFinish::Return);

        handover.reset();
        swipe.begin(3, 2000ms, true, true, true);
        const auto flick = swipe.update({60, 0}, 2040ms, true);
        handover.update({1, 0}, {flick.offset, 0}, QPointF(0.7, 0));
        QCOMPARE(swipe.end(2045ms, false, true).finish, SwipeFinish::Right);
    }

    void fingerScrollUsesTheSameVisualHandover()
    {
        GestureHandover handover;
        SettlingSwipe scroll;
        scroll.openWindow();
        auto frame = scroll.axis(-30, false, true, true, 0ms, true, true, true);
        QCOMPARE(handover.update({1, 0}, {frame.offset, 0}, QPointF(0.8, 0)), QPointF(0.7, 0));
        frame = scroll.axis(15, false, true, true, 500ms, true, true, true);
        QCOMPARE(handover.update({1, 0}, {frame.offset, 0}), QPointF(0.75, 0));
        QCOMPARE(scroll.axis(0, false, true, true, 1000ms, true, true, true).finish, SwipeFinish::Return);
    }
};
QTEST_GUILESS_MAIN(GestureHandoverTest)
#include "gesture_handover_test.moc"
