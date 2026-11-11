// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QPointF>
#include <optional>

namespace NaturalSwipe
{
// Visual origin only: inherited animation displacement must never count towards
// the new swipe's completion threshold or select a different logical desktop.
// Offsets are cumulative from the NEW input stream, not incremental deltas.
class GestureHandover
{
public:
    QPointF update(QPointF logicalOrigin, QPointF offset,
                   std::optional<QPointF> animationPosition = std::nullopt)
    {
        if (!m_origin) m_origin = animationPosition.value_or(logicalOrigin);
        return *m_origin + offset;
    }

    void reset() { m_origin.reset(); }

private:
    std::optional<QPointF> m_origin;
};
}
