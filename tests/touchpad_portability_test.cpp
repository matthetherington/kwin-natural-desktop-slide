// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "swipe_tracker.h"
#include <QTest>
#include <cmath>
using namespace NaturalSwipe;
using namespace std::chrono_literals;

class TouchpadPortabilityTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void eventRatesAndSensitivity_data()
    {
        QTest::addColumn<int>("intervalMs");
        QTest::addColumn<double>("distance");
        QTest::addColumn<int>("direction");
        for (int interval : {1, 4, 8, 10, 20}) {
            for (double distance : {100.0, 300.0, 900.0}) {
                for (int direction : {-1, 1}) {
                    const auto name = QByteArray::number(interval) + "ms-"
                        + QByteArray::number(distance) + "units-" + QByteArray::number(direction);
                    QTest::newRow(name.constData()) << interval << distance << direction;
                }
            }
        }
    }
    void eventRatesAndSensitivity()
    {
        QFETCH(int, intervalMs);
        QFETCH(double, distance);
        QFETCH(int, direction);
        GestureSettings settings;
        settings.distancePerDesktop = distance;
        for (bool flick : {false, true}) {
            SwipeTracker tracker;
            tracker.begin(3, 0ms, settings);
            const int duration = flick ? 40 : 1000;
            const double fraction = flick ? 0.2 : 0.6;
            for (int time = intervalMs; time <= duration; time += intervalMs) {
                // Synthetic normalized streams, not recordings of named hardware.
                const double dx = direction * distance * fraction * intervalMs / duration;
                tracker.update({dx, dx * 0.05}, std::chrono::milliseconds(time));
            }
            QVERIFY(std::abs(tracker.progress() - direction * fraction) < 1e-10);
            tracker.end(std::chrono::milliseconds(duration + (flick ? 0 : 200)), true);
            QCOMPARE(tracker.releaseDirection(), direction);
            QCOMPARE(tracker.flickRelease(), flick);
        }
    }
    void coalescedAndIrregularEvents()
    {
        SwipeTracker tracker;
        tracker.begin(3, 0ms);
        for (int time : {2, 2, 9, 17, 17, 38, 40}) tracker.update({9, 0.3}, std::chrono::milliseconds(time));
        QVERIFY(std::isfinite(tracker.velocity()));
        tracker.end(40ms, true);
        QCOMPARE(tracker.releaseDirection(), 1);
        tracker.begin(3, 100ms);
        tracker.update({60, 0}, 140ms);
        tracker.cancel(); // Contact loss must not complete a desktop switch.
        QCOMPARE(tracker.releaseDirection(), 0);
    }
};
QTEST_GUILESS_MAIN(TouchpadPortabilityTest)
#include "touchpad_portability_test.moc"
