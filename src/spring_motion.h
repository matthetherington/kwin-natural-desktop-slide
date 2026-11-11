/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Matt Hetherington
    SPDX-License-Identifier: GPL-2.0-or-later

    Adapted from KWin v6.8.2 src/effect/springmotion.{h,cpp}.
    Project-owned, header-only solver; no KWin ABI dependency. See docs/UPSTREAM.md.
*/
#pragma once
#include <chrono>
#include <cmath>

namespace NaturalSwipe
{
class SpringMotion
{
public:
    SpringMotion(double springConstant = 200.0, double dampingRatio = 1.1)
        : m_springConstant(springConstant)
        , m_dampingRatio(dampingRatio)
        , m_dampingCoefficient(2 * std::sqrt(springConstant) * dampingRatio)
    {
    }

    bool isMoving() const
    {
        return std::fabs(position() - anchor()) > m_epsilon || std::fabs(velocity()) > m_epsilon;
    }
    double position() const { return interpolate(m_prev.position, m_next.position); }
    double velocity() const { return interpolate(m_prev.velocity, m_next.velocity); }
    double anchor() const { return m_anchor; }
    double springConstant() const { return m_springConstant; }
    double dampingRatio() const { return m_dampingRatio; }
    double epsilon() const { return m_epsilon; }
    void setAnchor(double anchor) { m_anchor = anchor; }
    void setEpsilon(double epsilon) { m_epsilon = epsilon; }
    void setPosition(double position)
    {
        m_next = {position, velocity()};
        m_t = 1.0;
    }
    void setVelocity(double velocity)
    {
        m_next = {position(), velocity};
        m_t = 1.0;
    }
    void advance(std::chrono::milliseconds delta)
    {
        if (!isMoving()) return;
        if (std::isinf(m_springConstant)) {
            // Also reset interpolation: disabling animations must settle even
            // when the previous frame was between integration samples.
            m_prev = m_next = {m_anchor, 0.0};
            m_t = 1.0;
            return;
        }
        if (delta.count() <= 0) return;
        const double steps = (delta.count() / 1000.0) / m_timestep;
        for (m_t += steps; m_t > 1.0; m_t -= 1.0) {
            m_prev = m_next;
            m_next = integrate(m_next, m_timestep);
        }
    }

private:
    struct State { double position; double velocity; };
    struct Slope { double dp; double dv; };
    double interpolate(double a, double b) const { return a * (1 - m_t) + b * m_t; }
    Slope evaluate(const State &state, double dt, const Slope &slope) const
    {
        const State next{state.position + slope.dp * dt, state.velocity + slope.dv * dt};
        const double springForce = (m_anchor - next.position) * m_springConstant;
        const double dampingForce = -next.velocity * m_dampingCoefficient;
        return {next.velocity, springForce + dampingForce};
    }
    State integrate(const State &state, double dt) const
    {
        // RK4 integration of the mass-spring-damper equation, as in upstream.
        const Slope k1 = evaluate(state, 0.0, {0, 0});
        const Slope k2 = evaluate(state, 0.5 * dt, k1);
        const Slope k3 = evaluate(state, 0.5 * dt, k2);
        const Slope k4 = evaluate(state, dt, k3);
        const double dpdt = 1.0 / 6.0 * (k1.dp + 2 * k2.dp + 2 * k3.dp + k4.dp);
        const double dvdt = 1.0 / 6.0 * (k1.dv + 2 * k2.dv + 2 * k3.dv + k4.dv);
        return {state.position + dpdt * dt, state.velocity + dvdt * dt};
    }
    State m_prev{0, 0};
    State m_next{0, 0};
    double m_t = 1.0;
    const double m_timestep = 1.0 / 100.0;
    double m_anchor = 0;
    double m_springConstant;
    double m_dampingRatio;
    double m_dampingCoefficient;
    double m_epsilon = 1.0;
};
}
