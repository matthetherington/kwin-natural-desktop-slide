// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "window_adapter.h"
#include <QTest>
using namespace NaturalSwipe;

class WindowAdapterTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void exactDockIdentity()
    {
        for (const auto &identity : {QString("plasmashell"), QString("org.kde.plasmashell")}) {
            QVERIFY(panelFromWindow(true, identity, "eDP-1", {}, {}).isPlasmaPanel);
            QVERIFY(!panelFromWindow(false, identity, "eDP-1", {}, {}).isPlasmaPanel);
        }
        for (const auto &identity : {QString(""), QString("other-dock"), QString("fake.plasmashell"),
                                     QString("plasmashell plasmashell")}) {
            QVERIFY(!panelFromWindow(true, identity, "eDP-1", {}, {}).isPlasmaPanel);
        }
    }
    void outputLocalLogicalCoordinates()
    {
        const auto panel = panelFromWindow(true, "plasmashell", "DP-1",
            {-1908, 150, 48, 800}, {-1920, 50, 1920, 1080});
        QCOMPARE(panel.geometry, QRectF(12, 100, 48, 800));
        QCOMPARE(panel.outputGeometry, QRectF(0, 0, 1920, 1080));
        PanelRule rule;
        rule.edge = Edge::Left;
        rule.output = "DP-1";
        rule.action = PanelAction::Slide;
        QCOMPARE(selectPanel(panel, {rule}).action, PanelAction::Slide);
        const auto moved = panelFromWindow(true, "plasmashell", "DP-1",
            {2045, 300, 48, 800}, {2033, 200, 1920, 1080});
        QCOMPARE(selectPanel(moved, {rule}).action, PanelAction::Slide);
    }
    void fractionalCoordinatesStayFractional()
    {
        const auto panel = panelFromWindow(true, "plasmashell", "eDP-1",
            {12.5, 100.25, 48.5, 800.5}, {0, 0, 2033.5, 1270.5});
        QCOMPARE(panel.geometry.x(), 12.5);
        QCOMPARE(panel.outputGeometry.width(), 2033.5);
    }
};
QTEST_GUILESS_MAIN(WindowAdapterTest)
#include "window_adapter_test.moc"
