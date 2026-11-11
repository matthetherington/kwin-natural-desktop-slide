// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include <QJsonObject>
#include <QPluginLoader>
#include <QTest>
#include "../effect/compat/springmotion.h"

class PluginMetadataTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void metadataOnly()
    {
        // Read embedded metadata without loading code or constructing an effect.
        QPluginLoader loader(QStringLiteral(EFFECT_PATH));
        const auto metadata = loader.metaData()["MetaData"].toObject();
        const auto plugin = metadata["KPlugin"].toObject();
        QCOMPARE(plugin["Id"].toString(), QString("kwin_natural_slide"));
        QCOMPARE(plugin["EnabledByDefault"].toBool(true), false);
        QCOMPARE(plugin["License"].toString(), QString("GPL-2.0-or-later"));
        QCOMPARE(plugin["Website"].toString(), QString("https://github.com/matthetherington/kwin-virtual-desktop-natural-swipe"));
        QCOMPARE(plugin["BugReportUrl"].toString(), plugin["Website"].toString() + "/issues");
        if (HAS_SETTINGS_MODULE) {
            QCOMPARE(metadata["X-KDE-ConfigModule"].toString(), QString("kwin_natural_slide_config"));
        } else {
            QVERIFY(!metadata.contains("X-KDE-ConfigModule"));
        }
        QCOMPARE(metadata["org.kde.kwin.effect"].toObject()["exclusiveGroup"].toString(), QString("desktop-animations"));
        QVERIFY(!loader.isLoaded());
    }
    void exportedSpringAbiSmokeTest()
    {
        // Exercise the pinned compatibility declaration without creating an effect.
        KWin::SpringMotion spring(300.0, 1.1);
        spring.setPosition(100);
        spring.setAnchor(2000);
        QCOMPARE(spring.position(), 100.0);
        QVERIFY(spring.isMoving());
        for (int frame = 0; frame < 600; ++frame) spring.advance(std::chrono::milliseconds(16));
        QVERIFY(!spring.isMoving());
        QVERIFY(qAbs(spring.position() - 2000) <= spring.epsilon());
    }
};
QTEST_GUILESS_MAIN(PluginMetadataTest)
#include "plugin_metadata_test.moc"
