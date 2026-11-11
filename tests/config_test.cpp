// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "config.h"

#include <QTest>

using namespace NaturalSwipe;

class ConfigTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void validConfigAndLayoutChanges()
    {
        ConfigStore store;
        QString error = "stale error";
        QVERIFY(store.reload(R"({"schemaVersion":1,"defaultAction":"fixed","rules":[
            {"name":"left","output":"eDP-1","edge":"left","maxInset":12,
             "geometryBounds":[0,0,100,1080],"action":"slide"}]})", &error));
        QVERIFY(error.isEmpty());
        QCOMPARE(store.rules().size(), 1);
        PanelInfo panel{true, "eDP-1", {12, 100, 48, 800}, {0, 0, 1920, 1080}};
        QCOMPARE(selectPanel(panel, store.rules()).action, PanelAction::Slide);
        panel.output = "DP-1";
        QCOMPARE(selectPanel(panel, store.rules()).action, PanelAction::Fixed);
        panel.output = "eDP-1";
        panel.outputGeometry.setHeight(600);
        QCOMPARE(selectPanel(panel, store.rules()).action, PanelAction::Fixed);
    }

    void malformedConfig_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("syntax") << QByteArray("{");
        QTest::newRow("array") << QByteArray("[]");
        QTest::newRow("missing-version") << QByteArray(R"({"rules":[]})");
        QTest::newRow("unsupported-version") << QByteArray(R"({"schemaVersion":2,"rules":[]})");
        QTest::newRow("fractional-version") << QByteArray(R"({"schemaVersion":1.1,"rules":[]})");
        QTest::newRow("root-typo") << QByteArray(R"({"schemaVersion":1,"rules":[],"rulse":[]})");
        QTest::newRow("unsafe-default") << QByteArray(R"({"schemaVersion":1,"defaultAction":"slide","rules":[]})");
        QTest::newRow("wrong-rules-type") << QByteArray(R"({"schemaVersion":1,"rules":{}})");
        QTest::newRow("non-object-rule") << QByteArray(R"({"schemaVersion":1,"rules":[42]})");
        const QByteArray prefix = R"({"schemaVersion":1,"rules":[)";
        const QByteArray suffix = "]}";
        QTest::newRow("unknown-rule-key") << prefix + R"({"action":"slide","edeg":"left"})" + suffix;
        QTest::newRow("missing-action") << prefix + "{}" + suffix;
        QTest::newRow("bad-action") << prefix + R"({"action":true})" + suffix;
        QTest::newRow("empty-output") << prefix + R"({"action":"slide","output":" "})" + suffix;
        QTest::newRow("null-output") << prefix + R"({"action":"slide","output":null})" + suffix;
        QTest::newRow("bad-edge") << prefix + R"({"action":"slide","edge":"diagonal"})" + suffix;
        QTest::newRow("negative-inset") << prefix + R"({"action":"slide","edge":"left","maxInset":-1})" + suffix;
        QTest::newRow("string-inset") << prefix + R"({"action":"slide","edge":"left","maxInset":"12"})" + suffix;
        QTest::newRow("inset-without-edge") << prefix + R"({"action":"slide","maxInset":12})" + suffix;
        QTest::newRow("bad-bounds") << prefix + R"({"action":"slide","geometryBounds":[0,0,-1,4]})" + suffix;
        QTest::newRow("short-bounds") << prefix + R"({"action":"slide","geometryBounds":[0,0,4]})" + suffix;
        QTest::newRow("null-bounds") << prefix + R"({"action":"slide","geometryBounds":null})" + suffix;
    }

    void malformedConfig()
    {
        QFETCH(QByteArray, json);
        ConfigStore store;
        QString error;
        QVERIFY(!store.reload(json, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(store.rules().isEmpty());
    }

    void reloadIsAtomic()
    {
        ConfigStore store;
        QString error;
        QVERIFY(store.reload(R"({"schemaVersion":1,"rules":[{"action":"slide"}]})"));
        QVERIFY(!store.reload(R"({"schemaVersion":1,"rules":[{"action":"fixed"},{"action":"invalid"}]})", &error));
        QCOMPARE(store.rules().size(), 1);
        QCOMPARE(store.rules()[0].action, PanelAction::Slide);
        QVERIFY(error.contains("rules[1].action"));
        QVERIFY(!store.loadFile(QStringLiteral("/nonexistent/natural-swipe-config.json"), &error));
        QCOMPARE(store.rules()[0].action, PanelAction::Slide);
        QVERIFY(store.reload(R"({"schemaVersion":1,"rules":[]})", &error));
        QVERIFY(store.rules().isEmpty());
        QVERIFY(error.isEmpty());
    }

    void orderedRules()
    {
        ConfigStore store;
        QVERIFY(store.reload(R"({"schemaVersion":1,"rules":[
            {"edge":"left","action":"fixed"},{"action":"slide"}]})"));
        PanelInfo panel{true, "eDP-1", {12, 100, 48, 800}, {0, 0, 1920, 1080}};
        QCOMPARE(selectPanel(panel, store.rules()).action, PanelAction::Fixed);
        panel.geometry.moveLeft(1860);
        QCOMPARE(selectPanel(panel, store.rules()).action, PanelAction::Slide);
    }
};

QTEST_GUILESS_MAIN(ConfigTest)
#include "config_test.moc"
