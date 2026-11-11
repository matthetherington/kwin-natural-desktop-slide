// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "swipe_session.h"
#include <optional>

namespace NaturalSwipe
{

// Horizontal finger-scroll streams may begin only in an adapter-authorized
// settling window or bounded edge grace. Once owned, retain through axis-stop, even if the
// window closes or context is interrupted. A zero horizontal delta is libinput's
// scroll-stop, not a motion sample; it releases using the ordinary swipe policy.
class SettlingSwipe
{
public:
    void openWindow() { m_window = true; m_edgeStart.reset(); }
    void openEdgeWindow(SwipeTracker::Time released, std::chrono::milliseconds grace = edgeGrace)
    {
        m_window = grace.count() > 0;
        m_edgeStart = released;
        m_edgeGrace = grace;
    }
    void closeWindow() { m_window = false; m_edgeStart.reset(); }
    bool windowOpen() const { return m_window; }
    bool windowOpen(SwipeTracker::Time time) const
    {
        return m_window && (!m_edgeStart || (time >= *m_edgeStart && time - *m_edgeStart < m_edgeGrace));
    }
    bool edgeWindow() const { return m_window && m_edgeStart.has_value(); }
    static constexpr auto edgeGrace = std::chrono::milliseconds(400);
    bool owned() const { return m_session.owned(); }
    bool draining() const { return m_session.draining(); }
    const SwipeTracker &tracker() const { return m_session.tracker(); }
    SwipeFrame axis(double delta, bool inverted, bool horizontal, bool finger,
                    SwipeTracker::Time time, bool contextValid,
                    bool leftAvailable, bool rightAvailable, bool overscroll = false,
                    const GestureSettings &settings = {});
    SwipeFrame interrupt();
    SwipeFrame cancel(SwipeTracker::Time time);

private:
    SwipeSession m_session;
    bool m_window = false;
    std::optional<SwipeTracker::Time> m_edgeStart;
    std::chrono::milliseconds m_edgeGrace = edgeGrace;
};

}
