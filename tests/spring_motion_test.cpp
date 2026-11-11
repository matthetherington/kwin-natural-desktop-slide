// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "spring_motion.h"
#include "gesture_handover.h"
#include <QTest>
#include <limits>
using NaturalSwipe::SpringMotion;
using namespace std::chrono_literals;

class SpringMotionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void matchesNativeKWinTrajectory()
    {
        // Captured from Fedora KWin 6.8.2's exported solver: 16 ms frames,
        // spring constant 300, damping 1.1, position 100, anchor 2000.
        const double golden[][2] = {
            {163.45455246356971, 6578.2708667663901},
            {298.34682526879504, 9960.7055774951168},
            {469.84751418709618, 11190.56031686707},
            {650.19455857908747, 11216.834598713122},
            {825.96552368348773, 10611.913451529013},
            {987.64515663134625, 9655.5610740973898},
            {1133.9689427090811, 8592.6485767219601},
            {1262.838115521017, 7522.242010809241},
            {1374.7660361356595, 6512.5602942370924},
            {1472.1151256645724, 5586.1945349352909},
            {1554.2142665439185, 4770.6896584659125},
            {1624.8409602973047, 4050.7013156010771},
        };
        SpringMotion spring(300, 1.1);
        spring.setPosition(100);
        spring.setAnchor(2000);
        for (const auto &sample : golden) {
            spring.advance(16ms);
            QVERIFY(std::abs(spring.position() - sample[0]) < 1e-8);
            QVERIFY(std::abs(spring.velocity() - sample[1]) < 1e-8);
        }
        for (int i = 0; i < 600; ++i) spring.advance(16ms);
        QVERIFY(!spring.isMoving());
        QVERIFY(std::abs(spring.position() - 2000) <= spring.epsilon());
    }
    void zeroAndNegativeDurationPreserveState()
    {
        SpringMotion spring(300, 1.1);
        spring.setAnchor(1000);
        spring.advance(17ms);
        const auto position = spring.position(), velocity = spring.velocity();
        spring.advance(0ms);
        spring.advance(-1ms);
        QCOMPARE(spring.position(), position);
        QCOMPARE(spring.velocity(), velocity);
    }
    void disabledAnimationsSettleImmediately()
    {
        SpringMotion spring(std::numeric_limits<double>::infinity(), 1.1);
        spring.setPosition(100);
        spring.setVelocity(300);
        spring.setAnchor(2000);
        spring.advance(0ms);
        QCOMPARE(spring.position(), 2000.0);
        QCOMPARE(spring.velocity(), 0.0);
        QVERIFY(!spring.isMoving());
    }
    void interpolatedHandoverDiscardsOldVelocity()
    {
        for (double target : {0.0, 1.0}) {
            SpringMotion spring(300, 1.1);
            spring.setPosition(400);
            spring.setAnchor(target * 1000);
            spring.advance(17ms);
            const auto visible = spring.position();
            QVERIFY(spring.velocity() != 0);
            spring.setVelocity(0);
            QCOMPARE(spring.position(), visible);
            NaturalSwipe::GestureHandover handover;
            QCOMPARE(handover.update({target, 0}, {}, QPointF(visible / 1000, 0)), QPointF(visible / 1000, 0));
            const auto dragged = handover.update({target, 0}, {-0.04, 0});
            spring.setPosition(dragged.x() * 1000);
            QCOMPARE(spring.position(), dragged.x() * 1000);
            QCOMPARE(spring.velocity(), 0.0);
            for (int i = 0; i < 600; ++i) spring.advance(16ms);
            QVERIFY(!spring.isMoving());
        }
    }
};
QTEST_GUILESS_MAIN(SpringMotionTest)
#include "spring_motion_test.moc"
