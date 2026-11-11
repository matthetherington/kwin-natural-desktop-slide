// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "overscroll.h"
#include "swipe_session.h"
#include "settling_swipe.h"
#include <QTest>
#include <limits>

using namespace NaturalSwipe;
using namespace std::chrono_literals;

class OverscrollTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void boundedSymmetricResistance()
    {
        double previous = 0;
        for (double distance : {0.0, 0.01, 0.1, 0.5, 1.0, 10.0, 1000.0}) {
            const double offset = overscrollOffset(distance);
            QVERIFY(offset >= previous && offset <= overscrollLimit);
            QCOMPARE(overscrollOffset(-distance), -offset);
            previous = offset;
        }
        QCOMPARE(overscrollOffset(std::numeric_limits<double>::infinity()), 0.0);
        QCOMPARE(overscrollOffset(std::numeric_limits<double>::quiet_NaN()), 0.0);
    }

    void exposedStripDoesNotInventDesktop()
    {
        const QRectF output(100, 200, 1000, 600);
        QCOMPARE(overscrollGap(-0.1, 4, output), QRectF(100, 200, 100, 600));
        const auto right = overscrollGap(3.1, 4, output);
        QVERIFY(std::abs(right.x() - 1000) < 1e-8);
        QVERIFY(std::abs(right.width() - 100) < 1e-8);
        QVERIFY(overscrollGap(1.5, 4, output).isEmpty());
        QCOMPARE(overscrollGap(10, 4, output), QRectF(1000, 200, 100, 600));
        QCOMPARE(overscrollGap(10, 4, output, 0.2), QRectF(900, 200, 200, 600));
        QVERIFY(overscrollGap(1, 0, output).isEmpty());
    }

    void edgePreviewAlwaysReturnsAndDoesNotCommit()
    {
        for (int sign : {-1, 1}) {
            SwipeSession session;
            session.begin(3, 0ms, true, sign > 0, sign < 0, true, true);
            const auto frame = session.update({sign * 3000.0, 0}, 20ms, true);
            QVERIFY(frame.consume && frame.preview);
            QVERIFY(frame.offset * sign > 0 && std::abs(frame.offset) < overscrollLimit);
            QCOMPARE(session.end(25ms, false, true).finish, SwipeFinish::Return);
            QVERIFY(!session.boundaryRelease()); // Real preview replaces edge-only grace.
        }
    }

    void adjacentWallpaperRevealsTheCorrectImageEdge()
    {
        const QRectF output(100, 200, 1000, 600);
        for (double limit : {0.1, 0.2, 0.4}) {
            for (double amount : {0.01, 0.05, limit / 2, limit}) {
                const auto leftGap = overscrollGap(-amount, 4, output, limit);
                const double leftOffset = overscrollWallpaperOffset(-amount, 4, output.width(), limit);
                const auto leftSource = leftGap.translated(-leftOffset, 0);
                QVERIFY(std::abs(leftSource.right() - output.right()) < 1e-8);
                QVERIFY(std::abs(leftSource.left() - (output.right() - amount * output.width())) < 1e-8);
                QVERIFY(leftOffset < 0); // The adjacent tile mostly remains offscreen to the left.

                const auto rightGap = overscrollGap(3 + amount, 4, output, limit);
                const double rightOffset = overscrollWallpaperOffset(3 + amount, 4, output.width(), limit);
                const auto rightSource = rightGap.translated(-rightOffset, 0);
                QVERIFY(std::abs(rightSource.left() - output.left()) < 1e-8);
                QVERIFY(std::abs(rightSource.right() - (output.left() + amount * output.width())) < 1e-8);
            }
        }
        // More drag moves the left-hand neighbour farther right, revealing
        // progressively more of its right-hand image, not a fixed crop.
        QVERIFY(overscrollWallpaperOffset(-0.08, 4, 1000) > overscrollWallpaperOffset(-0.04, 4, 1000));
        QCOMPARE(overscrollWallpaperOffset(1.5, 4, 1000), 0.0);
        QCOMPARE(overscrollWallpaperOffset(-0.1, 0, 1000), 0.0);
        QCOMPARE(overscrollWallpaperOffset(-0.1, 4, std::numeric_limits<double>::infinity()), 0.0);
    }

    void reverseIntoAvailableNeighbour()
    {
        SwipeSession session;
        session.begin(3, 0ms, true, true, false, true, true);
        QVERIFY(session.update({300, 0}, 20ms, true).offset > 0);
        QCOMPARE(session.update({-450, 0}, 40ms, true).offset, -0.5);
        QCOMPARE(session.end(45ms, false, true).finish, SwipeFinish::Left);
    }

    void disabledAndInterruptedRemainSafe()
    {
        SwipeSession session;
        session.begin(3, 0ms, true, true, false);
        QVERIFY(!session.update({100, 0}, 10ms, true).preview);
        session.end(20ms, false, true);
        session.begin(3, 100ms, true, true, false, true, true);
        session.update({100, 0}, 110ms, true);
        QCOMPARE(session.interrupt().finish, SwipeFinish::Return);
        QVERIFY(session.draining());
        QCOMPARE(session.end(120ms, false, true).finish, SwipeFinish::None);
    }

    void singleDesktopCanBounceWithoutSwitching()
    {
        SwipeSession session;
        session.begin(3, 0ms, true, false, false, true, true);
        QVERIFY(!session.draining());
        QVERIFY(session.update({100, 0}, 20ms, true).preview);
        QCOMPARE(session.end(25ms, false, true).finish, SwipeFinish::Return);
    }

    void fingerScrollCanOverscrollAndReturn()
    {
        SettlingSwipe swipe;
        swipe.openWindow();
        const auto frame = swipe.axis(100, false, true, true, 0ms, true, true, false, true);
        QVERIFY(frame.preview && frame.offset > 0 && frame.offset < overscrollLimit);
        QCOMPARE(swipe.axis(0, false, true, true, 50ms, true, true, false, true).finish, SwipeFinish::Return);
        swipe.openWindow(); // Its return animation may admit the next swipe.
        QVERIFY(swipe.axis(-60, false, true, true, 100ms, true, true, false, true).preview);
    }
};
QTEST_GUILESS_MAIN(OverscrollTest)
#include "overscroll_test.moc"
