// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "gesture_settings.h"
#include "swipe_tracker.h"
#include "swipe_session.h"
#include "settling_swipe.h"
#include "animation_speed.h"
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <limits>

using namespace NaturalSwipe;
using namespace std::chrono_literals;

class GestureSettingsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void tunedDefaultsAndRoundTrip()
    {
        GestureSettingsStore store;
        QVERIFY(store.reload(R"({"schemaVersion":1})"));
        QCOMPARE(store.settings(), GestureSettings{});
        QCOMPARE(store.settings().slowSwipeThreshold, 0.3);
        QCOMPARE(store.settings().overscrollLimit, 0.1);
        QCOMPARE(store.settings().overscrollLimit, overscrollLimit);
        QCOMPARE(store.settings().overscrollBackground, QString("black"));
        QCOMPARE(store.settings().overscrollColor, QString("#000000"));
        QCOMPARE(store.settings().flickSettleSpeed, 1.0);
        QCOMPARE(store.settings().edgeGraceMs, 400);
        auto settings = store.settings();
        settings.overscrollBackground = "wallpaper";
        settings.overscrollColor = "#5aB3eF";
        settings.flickSettleSpeed = 2;
        settings.edgeGraceMs = 0;
        QVERIFY(store.reload(QJsonDocument(gestureSettingsJson(settings)).toJson()));
        QCOMPARE(store.settings(), settings);
        QTemporaryDir dir;
        const auto missing = dir.filePath("missing.json");
        QVERIFY(store.loadFile(missing));
        QCOMPARE(store.settings(), GestureSettings{});
        QVERIFY(!QFile::exists(missing));
    }

    void savedOverscrollLimitIsPreserved()
    {
        GestureSettingsStore store;
        QVERIFY(store.reload(R"({"schemaVersion":1,"overscrollLimit":0.2})"));
        QCOMPARE(store.settings().overscrollLimit, 0.2);
        const auto saved = QJsonDocument(gestureSettingsJson(store.settings())).toJson();
        QVERIFY(store.reload(saved));
        QCOMPARE(store.settings().overscrollLimit, 0.2);
        QVERIFY(store.reload(R"({"schemaVersion":1})"));
        QCOMPARE(store.settings().overscrollLimit, 0.1);
    }

    void invalidValuesRetainWholePreviousSettings()
    {
        GestureSettingsStore store;
        GestureSettings custom;
        custom.slowSwipeThreshold = 0.4;
        const auto original = QJsonDocument(gestureSettingsJson(custom)).toJson();
        const QList<QByteArray> invalid{
            "[]", "{", R"({"schemaVersion":2})", R"({"schemaVersion":1,"typo":1})",
            R"({"schemaVersion":1,"slowSwipeThreshold":0})", R"({"schemaVersion":1,"slowSwipeThreshold":1})",
            R"({"schemaVersion":1,"distancePerDesktop":0})", R"({"schemaVersion":1,"flickVelocity":"fast"})",
            R"({"schemaVersion":1,"overscrollLimit":0.9})", R"({"schemaVersion":1,"overscrollEnabled":1})",
            R"({"schemaVersion":1,"overscrollBackground":"blue"})", R"({"schemaVersion":1,"followups":null})",
            R"({"schemaVersion":1,"settleSpeed":0})", R"({"schemaVersion":1,"flickSettleSpeed":5})",
            R"({"schemaVersion":1,"returnSpeed":-1})", R"({"schemaVersion":1,"edgeGraceMs":1.5})",
            R"({"schemaVersion":1,"edgeGraceMs":1501})",
            R"({"schemaVersion":1,"overscrollColor":"red"})",
            R"({"schemaVersion":1,"overscrollColor":"#123"})",
            R"({"schemaVersion":1,"overscrollColor":"#80112233"})",
            R"({"schemaVersion":1,"overscrollColor":"#gg0000"})",
            R"({"schemaVersion":1,"overscrollColor":"#000000\n"})",
            R"({"schemaVersion":1,"overscrollColor":null})"};
        for (const auto &bytes : invalid) {
            QVERIFY(store.reload(original));
            QString error;
            QVERIFY(!store.reload(bytes, &error));
            QVERIFY(!error.isEmpty());
            QCOMPARE(store.settings(), custom);
        }
    }

    void atomicSaveConflictBackupAndSymlink()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("gestures.json");
        QString error, backup;
        GestureSettings settings;
        QVERIFY(saveGestureSettings(path, false, {}, settings, &backup, &error));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto original = file.readAll();
        file.close();
        settings.overscrollBackground = "wallpaper";
        QVERIFY(saveGestureSettings(path, true, original, settings, &backup, &error));
        QFile previous(backup);
        QVERIFY(previous.open(QIODevice::ReadOnly));
        QCOMPARE(previous.readAll(), original);
        QVERIFY(!saveGestureSettings(path, true, original, {}, &backup, &error));
        QVERIFY(error.contains("changed"));
        const auto link = dir.filePath("link.json");
        QVERIFY(QFile::link(path, link));
        QVERIFY(!saveGestureSettings(link, true, {}, {}, &backup, &error));
        QVERIFY(error.contains("symlink"));
    }

    void trackerUsesFrozenThresholdSensitivityAndFlickSpeed()
    {
        SwipeTracker tracker;
        GestureSettings settings;
        settings.distancePerDesktop = 600;
        settings.slowSwipeThreshold = 0.2;
        tracker.begin(3, 0ms, settings);
        settings.slowSwipeThreshold = 0.9; // Does not alter the in-flight snapshot.
        tracker.update({150, 0}, 1000ms);
        QCOMPARE(tracker.progress(), 0.25);
        tracker.end(1200ms, true);
        QCOMPARE(tracker.releaseDirection(), 1);
        QVERIFY(!tracker.flickRelease());
        settings = {};
        settings.flickVelocity = 20;
        tracker.begin(3, 0ms, settings);
        tracker.update({60, 0}, 40ms);
        tracker.end(45ms, true);
        QCOMPARE(tracker.releaseDirection(), 0);
        tracker.begin(3, 0ms);
        tracker.update({60, 0}, 40ms);
        tracker.end(45ms, true);
        QVERIFY(tracker.flickRelease());
        settings.distancePerDesktop = std::numeric_limits<double>::quiet_NaN();
        tracker.begin(3, 0ms, settings);
        QCOMPARE(tracker.state(), SwipeTracker::State::Rejected);
    }

    void configuredOverscrollAndEdgeGrace()
    {
        GestureSettings settings;
        settings.overscrollLimit = 0.4;
        SwipeSession swipe;
        swipe.begin(3, 0ms, true, true, false, true, true, settings);
        const auto frame = swipe.update({3000, 0}, 20ms, true);
        QVERIFY(frame.offset > 0.2 && frame.offset < 0.4);
        QCOMPARE(swipe.end(30ms, false, true).finish, SwipeFinish::Return);
        SettlingSwipe scroll;
        scroll.openEdgeWindow(0ms, 100ms);
        QVERIFY(scroll.windowOpen(99ms));
        QVERIFY(!scroll.windowOpen(100ms));
        scroll.openEdgeWindow(0ms, 0ms);
        QVERIFY(!scroll.windowOpen());
        settings.overscrollEnabled = false;
        swipe.begin(3, 100ms, true, true, false, true, true, settings);
        QVERIFY(!swipe.update({100, 0}, 120ms, true).preview);
    }

    void animationTimeScalePreservesBaselineAndFractionalTime()
    {
        AnimationSpeed speed;
        for (auto elapsed : {0ms, 1ms, 7ms, 16ms, 33ms}) QCOMPARE(speed.advance(elapsed), elapsed);
        speed.reset(2);
        QCOMPARE(speed.advance(16ms), 32ms);
        speed.reset(0.1);
        auto total = 0ms;
        for (int i = 0; i < 100; ++i) total += speed.advance(1ms);
        QVERIFY(total >= 9ms && total <= 10ms);
        speed.reset(0);
        QCOMPARE(speed.advance(16ms), 16ms);
    }
};
QTEST_GUILESS_MAIN(GestureSettingsTest)
#include "gesture_settings_test.moc"
