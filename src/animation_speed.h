// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <cmath>

namespace NaturalSwipe
{
// Preserve the existing spring and its velocity, scaling elapsed time instead.
// Carry sub-millisecond time so slow speeds cannot stall on high-refresh displays.
class AnimationSpeed
{
public:
    void reset(double speed)
    {
        m_speed = std::isfinite(speed) && speed >= 0.1 && speed <= 4 ? speed : 1;
        m_remainder = 0;
    }
    std::chrono::milliseconds advance(std::chrono::milliseconds elapsed)
    {
        if (elapsed.count() <= 0) return std::chrono::milliseconds(0);
        const long double scaled = elapsed.count() * m_speed + m_remainder;
        const auto whole = static_cast<std::chrono::milliseconds::rep>(std::floor(scaled));
        m_remainder = scaled - whole;
        return std::chrono::milliseconds(whole);
    }
private:
    long double m_speed = 1;
    long double m_remainder = 0;
};
}
