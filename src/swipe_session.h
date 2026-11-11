// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "swipe_tracker.h"

namespace NaturalSwipe
{

enum class SwipeFinish { None, Return, Left, Right };
struct SwipeFrame
{
    bool consume = false;
    bool preview = false;
    double offset = 0; // Viewport offset: negative = desktop to the left.
    SwipeFinish finish = SwipeFinish::None;
};

// Owns an entire three-finger stream before direction is known. Interruptions
// cancel the preview, then drain through end/cancel instead of leaking a tail
// to recognizers which never saw our begin. No compositor calls in this model.
class SwipeSession
{
public:
    SwipeFrame begin(int fingers, SwipeTracker::Time time, bool permitted,
                     bool leftAvailable, bool rightAvailable, bool previewPermitted = true,
                     bool overscroll = false, const GestureSettings &settings = {});
    SwipeFrame update(QPointF delta, SwipeTracker::Time time, bool contextValid);
    SwipeFrame end(SwipeTracker::Time time, bool cancelled, bool contextValid);
    SwipeFrame interrupt();
    bool owned() const { return m_owned; }
    bool draining() const { return m_draining; }
    bool boundaryRelease() const { return m_boundaryRelease; }
    const SwipeTracker &tracker() const { return m_tracker; }

private:
    SwipeTracker m_tracker;
    bool m_owned = false;
    bool m_draining = false;
    bool m_preview = false;
    bool m_leftAvailable = false;
    bool m_rightAvailable = false;
    bool m_boundaryRelease = false;
    bool m_overscroll = false;
};

}
