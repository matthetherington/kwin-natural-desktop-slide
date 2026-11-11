// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "swipe_tracker.h"

#include <algorithm>
#include <cmath>

namespace NaturalSwipe
{

void SwipeTracker::reset()
{
    m_state = State::Idle;
    m_fingers = 0;
    m_displacement = {};
    m_progress = 0;
    m_velocity = 0;
    m_releaseDirection = 0;
    m_flickRelease = false;
    m_lastTime = {};
    m_samples.clear();
}

void SwipeTracker::begin(int fingers, Time time, const GestureSettings &settings)
{
    reset();
    m_fingers = fingers;
    if (fingers != 3 || time.count() < 0 || !validGestureSettings(settings)) {
        m_state = State::Rejected;
        return;
    }
    m_state = State::Pending;
    m_settings = settings;
    m_lastTime = time;
    sample(time);
}

void SwipeTracker::sample(Time time)
{
    // Equal timestamps contribute displacement without creating a zero dt.
    if (!m_samples.empty() && m_samples.back().time == time) {
        m_samples.back().x = m_displacement.x();
    } else {
        m_samples.push_back({time, m_displacement.x()});
    }
    const Time cutoff = time - velocityWindow;
    // Keep one sample before the cutoff so crossing the window boundary does
    // not abruptly discard all velocity information at release.
    while (m_samples.size() > 2 && m_samples[1].time <= cutoff) m_samples.pop_front();
    while (m_samples.size() > maxSamples) m_samples.pop_front();
    m_velocity = 0;
    if (m_samples.size() > 1) {
        const auto &first = m_samples.front();
        Time startTime = first.time;
        double startX = first.x;
        if (first.time < cutoff) {
            const auto &second = m_samples[1];
            const double fraction = double((cutoff - first.time).count())
                / double((second.time - first.time).count());
            startX = (1.0 - fraction) * first.x + fraction * second.x;
            startTime = cutoff;
        }
        const double seconds = std::chrono::duration<double>(time - startTime).count();
        m_velocity = (m_displacement.x() - startX) / m_settings.distancePerDesktop / seconds;
    }
}

void SwipeTracker::update(QPointF delta, Time time)
{
    if (!active()) return;
    const QPointF next = m_displacement + delta;
    if (time < m_lastTime || !std::isfinite(delta.x()) || !std::isfinite(delta.y())
        || !std::isfinite(next.x()) || !std::isfinite(next.y())) {
        cancel();
        return;
    }
    m_lastTime = time;
    m_displacement = next;
    sample(time);
    if (!std::isfinite(m_velocity)) {
        cancel();
        return;
    }
    if (m_state == State::Pending) {
        if (std::hypot(next.x(), next.y()) < directionThreshold) return;
        // Inclusive 45-degree acquisition cone. Once horizontal, lock the axis
        // so that reversing or curving the gesture does not cancel it midway.
        if (std::abs(next.x()) < std::abs(next.y())) {
            m_state = State::Rejected;
            m_velocity = 0;
            m_samples.clear();
            return;
        }
        m_state = State::Horizontal;
    }
    m_progress = std::clamp(next.x() / m_settings.distancePerDesktop, -1.0, 1.0);
}

void SwipeTracker::end(Time time, bool allowFlick)
{
    if (!active()) return;
    if (time < m_lastTime) {
        cancel();
        return;
    }
    sample(time); // A pause before release must age the velocity estimate.
    // Momentum may complete a short deliberate flick, but never select the
    // opposite side of the visible preview. Horizontal acquisition supplies
    // the motion threshold; a second displacement floor vetoes valid short
    // flicks. Release speed is an alternative to drag distance, not another
    // projected-distance test. Pauses age velocity above; pending gestures
    // cannot commit.
    const bool flick = allowFlick
        && m_progress * m_velocity > 0
        && std::abs(m_velocity) >= m_settings.flickVelocity;
    if (m_state == State::Horizontal && (std::abs(m_progress) >= m_settings.slowSwipeThreshold || flick)) {
        m_releaseDirection = m_progress > 0 ? 1 : -1;
        m_flickRelease = flick;
    }
    m_state = State::Finished;
    m_samples.clear();
}

void SwipeTracker::cancel()
{
    if (!active()) return;
    m_state = State::Cancelled;
    m_releaseDirection = 0;
    m_velocity = 0;
    m_samples.clear();
    // Keep the last finite progress for a future controller's return animation.
}

const char *swipeStateName(SwipeTracker::State state)
{
    switch (state) {
    case SwipeTracker::State::Idle: return "idle";
    case SwipeTracker::State::Pending: return "pending";
    case SwipeTracker::State::Horizontal: return "horizontal";
    case SwipeTracker::State::Rejected: return "rejected";
    case SwipeTracker::State::Finished: return "finished";
    case SwipeTracker::State::Cancelled: return "cancelled";
    }
    return "unknown";
}

}
