// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "gesture_settings.h"

#include <QPointF>
#include <chrono>
#include <cstddef>
#include <deque>

namespace NaturalSwipe
{

// Pure input model: no KWin, desktop changes, event consumption or rendering.
// Progress follows raw horizontal motion; the future controller must map its
// sign to viewport movement/desktop direction explicitly.
class SwipeTracker
{
public:
    using Time = std::chrono::microseconds;
    enum class State { Idle, Pending, Horizontal, Rejected, Finished, Cancelled };

    void begin(int fingers, Time time, const GestureSettings &settings = {});
    void update(QPointF delta, Time time);
    void end(Time time, bool allowFlick = false);
    void cancel();
    void reset();

    State state() const { return m_state; }
    bool active() const { return m_state == State::Pending || m_state == State::Horizontal; }
    int fingers() const { return m_fingers; }
    QPointF displacement() const { return m_displacement; }
    double progress() const { return m_progress; } // Clamped to [-1, 1].
    double velocity() const { return m_velocity; } // Unclamped progress units/second.
    int releaseDirection() const { return m_releaseDirection; } // Raw sign; NOT a desktop index.
    bool flickRelease() const { return m_flickRelease; }
    const GestureSettings &settings() const { return m_settings; }

    // Tuned defaults retained for callers/tests; each begin snapshots settings.
    static constexpr double directionThreshold = 8.0;
    static constexpr double distancePerDesktop = 300.0;
    static constexpr double commitProgress = 0.3;
    static constexpr double minimumFlickVelocity = 1.0; // Desktops/second.
    static constexpr auto velocityWindow = std::chrono::milliseconds(100);
    static constexpr std::size_t maxSamples = 32;

private:
    void sample(Time time);
    struct Sample { Time time; double x; };
    State m_state = State::Idle;
    int m_fingers = 0;
    QPointF m_displacement;
    double m_progress = 0;
    double m_velocity = 0;
    int m_releaseDirection = 0;
    Time m_lastTime{};
    std::deque<Sample> m_samples;
    GestureSettings m_settings;
    bool m_flickRelease = false;
};

const char *swipeStateName(SwipeTracker::State state);

}
