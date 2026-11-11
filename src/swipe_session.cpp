// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "swipe_session.h"
#include "overscroll.h"

namespace NaturalSwipe
{

SwipeFrame SwipeSession::begin(int fingers, SwipeTracker::Time time, bool permitted,
                              bool leftAvailable, bool rightAvailable, bool previewPermitted, bool overscroll,
                              const GestureSettings &settings)
{
    if (m_owned) return interrupt();
    m_boundaryRelease = false;
    if (!permitted || fingers != 3) return {};
    m_owned = true;
    m_preview = false;
    m_leftAvailable = leftAvailable;
    m_rightAvailable = rightAvailable;
    m_overscroll = overscroll && settings.overscrollEnabled;
    m_tracker.begin(fingers, time, settings);
    m_draining = !previewPermitted || (!m_overscroll && !leftAvailable && !rightAvailable) || !m_tracker.active();
    if (m_draining) m_tracker.cancel();
    return {.consume = true};
}

SwipeFrame SwipeSession::update(QPointF delta, SwipeTracker::Time time, bool contextValid)
{
    if (!m_owned) return {};
    if (m_draining) return {.consume = true};
    if (!contextValid) return interrupt();
    m_tracker.update(delta, time);
    if (m_tracker.state() == SwipeTracker::State::Cancelled) return interrupt();
    if (m_tracker.state() != SwipeTracker::State::Horizontal) return {.consume = true};
    double offset = m_tracker.progress(); // Physical right selects the desktop to the right.
    if ((offset < 0 && !m_leftAvailable) || (offset > 0 && !m_rightAvailable)) {
        offset = m_overscroll ? overscrollOffset(m_tracker.displacement().x() / m_tracker.settings().distancePerDesktop,
                                               m_tracker.settings().overscrollLimit) : 0;
    }
    const bool preview = offset != 0 || m_preview;
    m_preview = preview;
    return {.consume = true, .preview = preview, .offset = offset};
}

SwipeFrame SwipeSession::interrupt()
{
    if (!m_owned) return {};
    const bool hadPreview = m_preview;
    m_tracker.cancel();
    m_preview = false;
    m_draining = true;
    return {.consume = true, .finish = hadPreview ? SwipeFinish::Return : SwipeFinish::None};
}

SwipeFrame SwipeSession::end(SwipeTracker::Time time, bool cancelled, bool contextValid)
{
    if (!m_owned) return {};
    SwipeFinish finish = m_preview ? SwipeFinish::Return : SwipeFinish::None;
    if (!m_draining && !cancelled && contextValid) {
        m_tracker.end(time, true);
        if (m_preview && m_tracker.state() == SwipeTracker::State::Finished) {
            if (m_tracker.releaseDirection() > 0 && m_rightAvailable) finish = SwipeFinish::Right;
            if (m_tracker.releaseDirection() < 0 && m_leftAvailable) finish = SwipeFinish::Left;
        }
    } else {
        m_tracker.cancel();
    }
    m_boundaryRelease = finish == SwipeFinish::None
        && m_tracker.state() == SwipeTracker::State::Finished
        && ((m_tracker.progress() > 0 && !m_rightAvailable)
            || (m_tracker.progress() < 0 && !m_leftAvailable));
    m_owned = m_draining = m_preview = false;
    return {.consume = true, .finish = finish};
}

}
