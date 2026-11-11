// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "panel_policy.h"

#include <QTest>

#include <limits>

using namespace NaturalSwipe;

namespace
{
PanelInfo leftPanel()
{
    return {true, QStringLiteral("eDP-1"), {12, 100, 48, 800}, {0, 0, 1920, 1080}};
}

PanelRule slidingRule(Edge edge)
{
    return {QStringLiteral("slide"), QStringLiteral("eDP-1"), edge,
            32, std::nullopt, PanelAction::Slide};
}
}

class PanelPolicyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultsAreFixed()
    {
        const auto decision = selectPanel(leftPanel(), {});
        QCOMPARE(decision.action, PanelAction::Fixed);
        QVERIFY(!decision.ruleIndex);
    }

    void onlyPanelsAreEligible()
    {
        auto panel = leftPanel();
        panel.isPlasmaPanel = false;
        QCOMPARE(selectPanel(panel, {slidingRule(Edge::Left)}).action, PanelAction::Fixed);
    }

    void matchesFloatingSidePanels()
    {
        const QVector<PanelRule> rules{slidingRule(Edge::Left), slidingRule(Edge::Right)};
        QCOMPARE(selectPanel(leftPanel(), rules).action, PanelAction::Slide);
        auto right = leftPanel();
        right.geometry.moveLeft(1860);
        const auto decision = selectPanel(right, rules);
        QCOMPARE(decision.action, PanelAction::Slide);
        QCOMPARE(decision.ruleIndex.value(), qsizetype(1));
    }

    void respectsOutputAndInset()
    {
        auto panel = leftPanel();
        panel.output = QStringLiteral("DP-1");
        QCOMPARE(selectPanel(panel, {slidingRule(Edge::Left)}).action, PanelAction::Fixed);
        panel = leftPanel();
        panel.geometry.moveLeft(33);
        QCOMPARE(selectPanel(panel, {slidingRule(Edge::Left)}).action, PanelAction::Fixed);
        panel.geometry.moveLeft(32);
        QCOMPARE(selectPanel(panel, {slidingRule(Edge::Left)}).action, PanelAction::Slide);
    }

    void orientationPreventsCornerMisclassification()
    {
        auto panel = leftPanel();
        panel.geometry = {0, 0, 1920, 48};
        QCOMPARE(selectPanel(panel, {slidingRule(Edge::Left)}).action, PanelAction::Fixed);
        QCOMPARE(selectPanel(panel, {slidingRule(Edge::Top)}).action, PanelAction::Slide);
        panel.geometry.moveTop(1032);
        QCOMPARE(selectPanel(panel, {slidingRule(Edge::Bottom)}).action, PanelAction::Slide);
    }

    void firstMatchWins()
    {
        auto fixed = slidingRule(Edge::Left);
        fixed.action = PanelAction::Fixed;
        const auto decision = selectPanel(leftPanel(), {fixed, slidingRule(Edge::Left)});
        QCOMPARE(decision.action, PanelAction::Fixed);
        QCOMPARE(decision.ruleIndex.value(), qsizetype(0));
    }

    void geometryDistinguishesPanelsOnSameEdge()
    {
        auto rule = slidingRule(Edge::Left);
        rule.geometryBounds = QRectF(0, 0, 100, 950);
        QCOMPARE(selectPanel(leftPanel(), {rule}).action, PanelAction::Slide);
        rule.geometryBounds = QRectF(0, 0, 100, 500);
        QCOMPARE(selectPanel(leftPanel(), {rule}).action, PanelAction::Fixed);
    }

    void wildcardRule()
    {
        PanelRule rule;
        rule.action = PanelAction::Slide;
        QCOMPARE(selectPanel(leftPanel(), {rule}).action, PanelAction::Slide);
    }

    void invalidInputsFailClosed()
    {
        const auto rule = slidingRule(Edge::Left);
        auto panel = leftPanel();
        panel.geometry.setWidth(0);
        QCOMPARE(selectPanel(panel, {rule}).action, PanelAction::Fixed);
        panel = leftPanel();
        panel.geometry.moveLeft(-1);
        QCOMPARE(selectPanel(panel, {rule}).action, PanelAction::Fixed);
        panel = leftPanel();
        panel.geometry.setX(std::numeric_limits<qreal>::quiet_NaN());
        QCOMPARE(selectPanel(panel, {rule}).action, PanelAction::Fixed);
        panel = leftPanel();
        panel.outputGeometry.moveLeft(1920);
        QCOMPARE(selectPanel(panel, {rule}).action, PanelAction::Fixed);
        auto invalidRule = rule;
        invalidRule.maxInset = -1;
        QCOMPARE(selectPanel(leftPanel(), {invalidRule}).action, PanelAction::Fixed);
        invalidRule.maxInset = std::numeric_limits<qreal>::infinity();
        QCOMPARE(selectPanel(leftPanel(), {invalidRule}).action, PanelAction::Fixed);
    }
};

QTEST_GUILESS_MAIN(PanelPolicyTest)
#include "panel_policy_test.moc"
