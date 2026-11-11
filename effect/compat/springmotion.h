/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Matt Hetherington
    SPDX-License-Identifier: GPL-2.0-or-later

    Declaration adapted from KWin v6.8.2 src/effect/springmotion.h on
    2026-11-11: comments shortened. Fedora exports this class from libkwin
    but omits its header in kwin-devel. Layout must match the pinned version.
*/
#pragma once
#if __has_include("effect/springmotion.h")
// Prefer the matching development package's declaration when it is installed.
#include "effect/springmotion.h"
#else
#include "kwin_export.h"
#include <QtGlobal>
#include <chrono>

namespace KWin
{
class KWIN_EXPORT SpringMotion
{
public:
    SpringMotion();
    SpringMotion(qreal springConstant, qreal dampingRatio);
    void advance(std::chrono::milliseconds delta);
    bool isMoving() const;
    qreal velocity() const;
    void setVelocity(qreal velocity);
    qreal position() const;
    void setPosition(qreal position);
    qreal anchor() const;
    void setAnchor(qreal anchor);
    qreal springConstant() const;
    qreal dampingRatio() const;
    qreal epsilon() const;
    void setEpsilon(qreal epsilon);
private:
    struct State { qreal position; qreal velocity; };
    struct Slope { qreal dp; qreal dv; };
    State integrate(const State &state, qreal dt);
    Slope evaluate(const State &state, qreal dt, const Slope &slope);
    State m_prev;
    State m_next;
    qreal m_t;
    qreal m_timestep;
    qreal m_anchor;
    qreal m_springConstant;
    qreal m_dampingRatio;
    qreal m_dampingCoefficient;
    qreal m_epsilon;
};
}
#endif
