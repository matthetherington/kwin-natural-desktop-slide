// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include <QJsonObject>
#include <QPluginLoader>
#include <QTest>

class ControllerMetadataTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void optInNonRenderingPlugin()
    {
        QPluginLoader loader(QStringLiteral(CONTROLLER_PATH));
        const auto metadata = loader.metaData()["MetaData"].toObject();
        const auto plugin = metadata["KPlugin"].toObject();
        QCOMPARE(plugin["Id"].toString(), QStringLiteral(CONTROLLER_ID));
        QCOMPARE(plugin["EnabledByDefault"].toBool(true), false);
        QCOMPARE(plugin["Category"].toString(), QString("Tools"));
        QVERIFY(!metadata.contains("org.kde.kwin.effect"));
        QVERIFY(!metadata.contains("X-KDE-ConfigModule"));
        QVERIFY(!loader.isLoaded());
    }
};
QTEST_GUILESS_MAIN(ControllerMetadataTest)
#include "controller_metadata_test.moc"
