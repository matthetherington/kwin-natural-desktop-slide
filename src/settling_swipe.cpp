// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "settling_swipe.h"
#include <cmath>

namespace NaturalSwipe
{

SwipeFrame SettlingSwipe::axis(double delta, bool inverted, bool horizontal, bool finger,
                               SwipeTracker::Time time, bool contextValid,
                               bool leftAvailable, bool rightAvailable, bool overscroll, const GestureSettings &settings)
{
    if (!horizontal || !finger) return {};
    if (!m_session.owned()) {
        if (!windowOpen(time) || !contextValid || delta == 0 || !std::isfinite(delta)) return {};
        // Three here selects the shared model's calibrated policy; the adapter
        // has admitted a finger-scroll stream, not measured three raw contacts.
        m_session.begin(3, time, true, leftAvailable, rightAvailable, true, overscroll, settings);
        closeWindow();
    }
    if (delta == 0) return m_session.end(time, false, contextValid);
    // Axis deltas already include natural scrolling; restore physical direction.
    return m_session.update({inverted ? -delta : delta, 0}, time, contextValid);
}

SwipeFrame SettlingSwipe::interrupt()
{
    closeWindow();
    return m_session.interrupt();
}

SwipeFrame SettlingSwipe::cancel(SwipeTracker::Time time)
{
    closeWindow();
    return m_session.end(time, true, false);
}

}
