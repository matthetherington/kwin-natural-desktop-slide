// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include <QJsonObject>
#include <QPluginLoader>
#include <QTest>
#include "../src/spring_motion.h"
#include "../src/gesture_handover.h"
#include "../effect/compat/runtime_version.h"

class PluginMetadataTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void mismatchedRuntimeIsRefused()
    {
        const auto previous = QCoreApplication::applicationVersion();
        QCoreApplication::setApplicationVersion(QStringLiteral(NDS_KWIN_BUILD_VERSION));
        QVERIFY(NaturalSwipe::runtimeMatchesBuild());
        QCoreApplication::setApplicationVersion("6.99.0");
        QVERIFY(!NaturalSwipe::runtimeMatchesBuild());
        QCoreApplication::setApplicationVersion(previous);
    }
    void metadataOnly()
    {
        // Read embedded metadata without loading code or constructing an effect.
        QPluginLoader loader(QStringLiteral(EFFECT_PATH));
        const auto metadata = loader.metaData()["MetaData"].toObject();
        const auto plugin = metadata["KPlugin"].toObject();
        QCOMPARE(plugin["Id"].toString(), QString(RENDERER_ID));
        QCOMPARE(plugin["EnabledByDefault"].toBool(true), false);
        QCOMPARE(plugin["License"].toString(), QString("GPL-2.0-or-later"));
        QCOMPARE(plugin["Website"].toString(), QString("https://github.com/matthetherington/kwin-natural-desktop-slide"));
        QCOMPARE(plugin["Name"].toString(), QString("Natural Desktop Slide (Experimental)"));
        QCOMPARE(plugin["BugReportUrl"].toString(), plugin["Website"].toString() + "/issues");
        if (HAS_SETTINGS_MODULE) {
            QCOMPARE(metadata["X-KDE-ConfigModule"].toString(), QString("kwin_natural_desktop_slide_config"));
        } else {
            QVERIFY(!metadata.contains("X-KDE-ConfigModule"));
        }
        QCOMPARE(metadata["org.kde.kwin.effect"].toObject()["exclusiveGroup"].toString(), QString("desktop-animations"));
        QVERIFY(!loader.isLoaded());
    }
    void localSpringSmokeTest()
    {
        NaturalSwipe::SpringMotion spring(300.0, 1.1);
        spring.setPosition(100);
        spring.setAnchor(2000);
        QCOMPARE(spring.position(), 100.0);
        QVERIFY(spring.isMoving());
        for (int frame = 0; frame < 600; ++frame) spring.advance(std::chrono::milliseconds(16));
        QVERIFY(!spring.isMoving());
        QVERIFY(qAbs(spring.position() - 2000) <= spring.epsilon());
    }

    void handoverFromRealInterpolatedSpringPosition()
    {
        for (double target : {0.0, 1.0}) {
            NaturalSwipe::SpringMotion spring(300.0, 1.1);
            spring.setPosition(400);
            spring.setAnchor(target * 1000);
            // Not a multiple of the spring's internal timestep: position()
            // includes interpolation, exactly as the renderer's paint path does.
            spring.advance(std::chrono::milliseconds(17));
            const QPointF visible(spring.position() / 1000, 0);
            NaturalSwipe::GestureHandover handover;
            QCOMPARE(handover.update({target, 0}, {}, visible), visible);
            const auto dragged = handover.update({target, 0}, {-0.04, 0});
            QCOMPARE(dragged, visible - QPointF(0.04, 0));
            // New release starts at the tracked point, not the selected desktop
            // or the preceding gesture's stale position/animation velocity.
            spring.setVelocity(0);
            spring.setPosition(dragged.x() * 1000);
            QCOMPARE(spring.position(), dragged.x() * 1000);
            QCOMPARE(spring.velocity(), 0.0);
            handover.reset();
            for (int frame = 0; frame < 600; ++frame) spring.advance(std::chrono::milliseconds(16));
            QVERIFY(!spring.isMoving());
        }
    }
};
QTEST_GUILESS_MAIN(PluginMetadataTest)
#include "plugin_metadata_test.moc"
