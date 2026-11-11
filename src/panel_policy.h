// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QRectF>
#include <QString>
#include <QVector>

#include <optional>

namespace NaturalSwipe
{

enum class PanelAction { Fixed, Slide };
enum class Edge { Left, Right, Top, Bottom };

// The KWin adapter must positively identify Plasma docks, not just plasmashell
// windows (which also include desktop backgrounds and dialogs).
struct PanelInfo
{
    bool isPlasmaPanel = false;
    QString output;
    QRectF geometry; // Logical, output-local coordinates.
    QRectF outputGeometry; // Origin must be (0, 0).
};

struct PanelRule
{
    QString name;
    std::optional<QString> output; // Unset means any output.
    std::optional<Edge> edge; // Unset means any edge/location.
    qreal maxInset = 32;
    std::optional<QRectF> geometryBounds; // Must fully contain the panel.
    PanelAction action = PanelAction::Fixed;
};

struct PanelDecision
{
    PanelAction action = PanelAction::Fixed;
    std::optional<qsizetype> ruleIndex;
};

// First matching rule wins. No rule may opt non-panel windows into sliding.
PanelDecision selectPanel(const PanelInfo &panel, const QVector<PanelRule> &rules);

} // namespace NaturalSwipe
