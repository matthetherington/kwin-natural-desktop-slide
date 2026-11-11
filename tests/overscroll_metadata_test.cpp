// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include <QJsonObject>
#include <QPluginLoader>
#include <QTest>

class OverscrollMetadataTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void separateOptInRenderer()
    {
        QPluginLoader loader(QStringLiteral(OVERSCROLL_PATH));
        const auto metadata = loader.metaData()["MetaData"].toObject();
        const auto plugin = metadata["KPlugin"].toObject();
        QCOMPARE(plugin["Id"].toString(), QStringLiteral(OVERSCROLL_ID));
        QVERIFY(plugin["Id"].toString() != "kwin_natural_slide");
        QCOMPARE(plugin["EnabledByDefault"].toBool(true), false);
        QVERIFY(!metadata.contains("X-KDE-ConfigModule"));
        QVERIFY(!loader.isLoaded());
    }
};
QTEST_GUILESS_MAIN(OverscrollMetadataTest)
#include "overscroll_metadata_test.moc"
