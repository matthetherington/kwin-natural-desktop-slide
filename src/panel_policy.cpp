// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "panel_policy.h"

#include <cmath>

namespace NaturalSwipe
{
namespace
{

bool finiteRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y())
        && std::isfinite(rect.width()) && std::isfinite(rect.height())
        && rect.width() > 0 && rect.height() > 0
        && std::isfinite(rect.right()) && std::isfinite(rect.bottom());
}

bool matchesEdge(const PanelInfo &panel, Edge edge, qreal maxInset)
{
    const auto &rect = panel.geometry;
    const auto &output = panel.outputGeometry;
    const bool vertical = rect.height() > rect.width();
    qreal inset = 0;
    switch (edge) {
    case Edge::Left:
        if (!vertical) return false;
        inset = rect.left() - output.left();
        break;
    case Edge::Right:
        if (!vertical) return false;
        inset = output.right() - rect.right();
        break;
    case Edge::Top:
        if (vertical) return false;
        inset = rect.top() - output.top();
        break;
    case Edge::Bottom:
        if (vertical) return false;
        inset = output.bottom() - rect.bottom();
        break;
    }
    return inset >= 0 && inset <= maxInset;
}

} // namespace

PanelDecision selectPanel(const PanelInfo &panel, const QVector<PanelRule> &rules)
{
    if (!panel.isPlasmaPanel || !finiteRect(panel.geometry)
        || !finiteRect(panel.outputGeometry)
        || panel.outputGeometry.topLeft() != QPointF(0, 0)
        || !panel.outputGeometry.contains(panel.geometry)) {
        return {};
    }

    for (qsizetype i = 0; i < rules.size(); ++i) {
        const auto &rule = rules[i];
        if (!std::isfinite(rule.maxInset) || rule.maxInset < 0) {
            continue;
        }
        if (rule.output && *rule.output != panel.output) {
            continue;
        }
        if (rule.geometryBounds && (!finiteRect(*rule.geometryBounds)
                                   || !rule.geometryBounds->contains(panel.geometry))) {
            continue;
        }
        if (rule.edge && !matchesEdge(panel, *rule.edge, rule.maxInset)) {
            continue;
        }
        return {rule.action, i};
    }
    return {};
}

} // namespace NaturalSwipe
