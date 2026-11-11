// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QRectF>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace NaturalSwipe
{
struct GestureOutput
{
    quintptr identity = 0; // Ephemeral output identity, never persisted.
    QRectF geometry;
    double scale = 1;
    int transform = 0;
    bool operator==(const GestureOutput &) const = default;
};

inline bool validOutputLayout(const QVector<GestureOutput> &outputs)
{
    if (outputs.isEmpty()) return false;
    QVector<quintptr> identities;
    for (const auto &output : outputs) {
        const auto &r = output.geometry;
        if (!output.identity || identities.contains(output.identity) || r.isEmpty()
            || !std::isfinite(r.x()) || !std::isfinite(r.y()) || !std::isfinite(r.width())
            || !std::isfinite(r.height()) || !std::isfinite(output.scale) || output.scale <= 0) return false;
        identities.append(output.identity);
    }
    return true;
}

inline QVector<quintptr> gestureOutputs(const QVector<GestureOutput> &outputs, quintptr active, bool independent)
{
    if (!validOutputLayout(outputs)) return {};
    QVector<quintptr> result;
    bool found = false;
    for (const auto &output : outputs) {
        found |= output.identity == active;
        if (!independent || output.identity == active) result.append(output.identity);
    }
    return found ? result : QVector<quintptr>{};
}

inline bool sameOutputLayout(QVector<GestureOutput> before, QVector<GestureOutput> after)
{
    if (!validOutputLayout(before) || !validOutputLayout(after)) return false;
    const auto order = [](const auto &a, const auto &b) { return a.identity < b.identity; };
    std::sort(before.begin(), before.end(), order);
    std::sort(after.begin(), after.end(), order);
    return before == after;
}
}
