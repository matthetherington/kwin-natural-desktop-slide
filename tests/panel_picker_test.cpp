// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "panel_picker.h"
#include "config.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>

using namespace NaturalSwipe;

namespace
{
QVariantMap pickedWindow()
{
    return {{"type", 2}, {"resourceClass", "plasmashell"}, {"x", -1908},
            {"y", 100}, {"width", 48}, {"height", 800}, {"uuid", "never-save-this"}};
}
PanelInfo panel() { return {true, "DP-1", {12, 100, 48, 800}, {0, 0, 1920, 1080}}; }
const QByteArray initial = R"({"schemaVersion":1,"defaultAction":"fixed","rules":[
    {"name":"both sides","output":"DP-1","edge":"left","action":"slide"},
    {"name":"other display","output":"eDP-1","action":"slide"}]})";
}

class PanelPickerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void selectActualDock()
    {
        PanelInfo selected;
        QString error;
        QVERIFY(pickedPanel(pickedWindow(), {{"DP-1", {-1920, 0, 1920, 1080}}}, selected, &error));
        QCOMPARE(selected.geometry, panel().geometry);
        QCOMPARE(selected.output, QString("DP-1"));
        QCOMPARE(selected.outputGeometry, panel().outputGeometry);
    }
    void rejectInvalidSelections()
    {
        PanelInfo selected;
        QString error;
        auto window = pickedWindow();
        window["type"] = 0;
        QVERIFY(!pickedPanel(window, {}, selected, &error));
        window = pickedWindow();
        window["resourceClass"] = "fake.plasmashell";
        QVERIFY(!pickedPanel(window, {}, selected, &error));
        window = pickedWindow();
        window.remove("width");
        QVERIFY(!pickedPanel(window, {}, selected, &error));
        window = pickedWindow();
        const QVector<OutputInfo> mirrored{{"DP-1", {-1920, 0, 1920, 1080}}, {"DP-2", {-1920, 0, 1920, 1080}}};
        QVERIFY(!pickedPanel(window, mirrored, selected, &error));
        QVERIFY(error.contains("overlapping"));
        QVERIFY(!pickedPanel(window, {{"eDP-1", {0, 0, 1920, 1080}}}, selected, &error));
        QVERIFY(error.contains("does not fit"));
    }
    void fixedChoiceOverridesBroadRuleAndPreservesOthers()
    {
        QByteArray updated;
        QString error;
        QVERIFY(panelOverride(initial, panel(), PanelAction::Fixed, updated, &error));
        QVERIFY(!updated.contains("uuid"));
        QVERIFY(!updated.contains("never-save-this"));
        ConfigStore config;
        QVERIFY(config.reload(updated));
        QCOMPARE(config.rules().size(), 3);
        QCOMPARE(config.rules()[1].name, QString("both sides"));
        QCOMPARE(config.rules()[2].name, QString("other display"));
        QCOMPARE(selectPanel(panel(), config.rules()).action, PanelAction::Fixed);
        auto another = panel();
        another.geometry.moveTop(0);
        QCOMPARE(selectPanel(another, config.rules()).action, PanelAction::Slide);
        QByteArray changed;
        QVERIFY(panelOverride(updated, panel(), PanelAction::Slide, changed, &error));
        QVERIFY(config.reload(changed));
        QCOMPARE(config.rules().size(), 3); // Re-picking does not accumulate duplicates.
        QCOMPARE(selectPanel(panel(), config.rules()).action, PanelAction::Slide);
    }
    void multipleChoicesAndContainingBounds()
    {
        const auto left = panel();
        auto right = panel();
        right.geometry.moveLeft(1860);
        QByteArray updated;
        QString error;
        QVERIFY(panelOverrides(initial, {{left, PanelAction::Fixed}, {right, PanelAction::Slide}}, updated, &error));
        ConfigStore config;
        QVERIFY(config.reload(updated));
        QCOMPARE(selectPanel(left, config.rules()).action, PanelAction::Fixed);
        QCOMPARE(selectPanel(right, config.rules()).action, PanelAction::Slide);
        QCOMPARE(config.rules().size(), 4);
        auto small = left;
        small.geometry.setHeight(300);
        // A smaller selected panel gets priority regardless of UI list order.
        for (const auto &choices : {QVector<PanelChoice>{{small, PanelAction::Fixed}, {left, PanelAction::Slide}},
                                    QVector<PanelChoice>{{left, PanelAction::Slide}, {small, PanelAction::Fixed}}}) {
            QVERIFY(panelOverrides(initial, choices, updated, &error));
            QVERIFY(config.reload(updated));
            QCOMPARE(selectPanel(left, config.rules()).action, PanelAction::Slide);
            QCOMPARE(selectPanel(small, config.rules()).action, PanelAction::Fixed);
        }
        auto invalid = right;
        invalid.isPlasmaPanel = false;
        updated = "untouched";
        QVERIFY(!panelOverrides(initial, {{left, PanelAction::Slide}, {invalid, PanelAction::Fixed}}, updated, &error));
        QCOMPARE(updated, QByteArray("untouched"));
    }
    void invalidConfigurationNeverReplaced()
    {
        QByteArray updated = "untouched";
        QString error;
        QVERIFY(!panelOverride("{}", panel(), PanelAction::Slide, updated, &error));
        QCOMPARE(updated, QByteArray("untouched"));
        auto invalid = panel();
        invalid.isPlasmaPanel = false;
        QVERIFY(!panelOverride(initial, invalid, PanelAction::Slide, updated, &error));
    }
    void atomicSaveBackupAndConflictDetection()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("new/panels.json");
        QString backup, error;
        QVERIFY(savePanelConfig(path, false, {}, initial, &backup, &error));
        QVERIFY(backup.isEmpty());
        QByteArray updated;
        QVERIFY(panelOverride(initial, panel(), PanelAction::Fixed, updated, &error));
        QVERIFY(savePanelConfig(path, true, initial, updated, &backup, &error));
        QFile old(backup);
        QVERIFY(old.open(QIODevice::ReadOnly));
        QCOMPARE(old.readAll(), initial);
        QVERIFY(!savePanelConfig(path, true, initial, initial, &backup, &error));
        QVERIFY(error.contains("changed in another program"));
        QFile saved(path);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QCOMPARE(saved.readAll(), updated);
        saved.close();
        QVERIFY(!savePanelConfig(path, false, {}, updated, &backup, &error));
        QVERIFY(error.contains("appeared/disappeared"));
        QVERIFY(!savePanelConfig(path, true, updated, "{}", &backup, &error));
    }
};
QTEST_GUILESS_MAIN(PanelPickerTest)
#include "panel_picker_test.moc"
