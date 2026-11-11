// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QRectF>
#include <algorithm>
#include <cmath>

namespace NaturalSwipe
{
inline constexpr double overscrollLimit = 0.1;

inline double overscrollOffset(double displacement, double limit = overscrollLimit)
{
    if (!std::isfinite(displacement) || !std::isfinite(limit) || limit <= 0) return 0;
    const double distance = std::abs(displacement);
    return std::copysign(limit * (distance / (distance + limit)), displacement);
}

// Exposed strip only, in logical output coordinates. No actual desktop is
// created: the renderer fills a solid colour or clips adjacent wallpaper here.
inline QRectF overscrollGap(double position, int columns, const QRectF &output, double limit = overscrollLimit)
{
    if (!std::isfinite(position) || !std::isfinite(limit) || limit <= 0 || columns < 1 || output.isEmpty()) return {};
    if (position < 0) {
        return {output.left(), output.top(), output.width() * std::min(-position, limit), output.height()};
    }
    if (position > columns - 1) {
        const double width = output.width() * std::min(position - (columns - 1), limit);
        return {output.right() - width, output.top(), width, output.height()};
    }
    return {};
}

// Place a wallpaper-only tile just beyond the grid, moving with the viewport.
// At the left edge its rightmost pixels enter first; at the right edge its
// leftmost pixels enter first. This is a translation of the existing wallpaper,
// not a crop stretched to fill the gap or a stationary backdrop.
inline double overscrollWallpaperOffset(double position, int columns, double outputWidth, double limit = overscrollLimit)
{
    if (!std::isfinite(position) || !std::isfinite(outputWidth) || !std::isfinite(limit)
        || limit <= 0 || outputWidth <= 0 || columns < 1) return 0;
    position = std::clamp(position, -limit, columns - 1 + limit);
    if (position < 0) return (-1 - position) * outputWidth;
    if (position > columns - 1) return (columns - position) * outputWidth;
    return 0;
}
}
