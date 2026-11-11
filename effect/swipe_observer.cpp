// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "swipe_observer.h"
#include "effect/effecthandler.h"
#include "input_event.h"

namespace KWin
{

void SwipeObserver::clear()
{
    m_tracker.reset();
    m_begins = m_updates = m_ends = m_cancels = 0;
}

void SwipeObserver::swipeGestureBegin(PointerSwipeGestureBeginEvent *event)
{
    if (effects->isScreenLocked()) return;
    ++m_begins;
    m_tracker.begin(event->fingerCount, event->time);
}

void SwipeObserver::swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event)
{
    if (effects->isScreenLocked()) return;
    ++m_updates;
    m_tracker.update(event->delta, event->time);
}

void SwipeObserver::swipeGestureEnd(PointerSwipeGestureEndEvent *event)
{
    if (effects->isScreenLocked()) return;
    ++m_ends;
    m_tracker.end(event->time);
}

void SwipeObserver::swipeGestureCancelled(PointerSwipeGestureCancelEvent *)
{
    if (effects->isScreenLocked()) return;
    ++m_cancels;
    m_tracker.cancel();
}

QJsonObject SwipeObserver::status() const
{
    if (effects->isScreenLocked()) {
        return {{"enabled", true}, {"mode", "observe-only"}, {"state", "suspended"}};
    }
    return {{"enabled", true}, {"mode", "observe-only"},
        {"state", NaturalSwipe::swipeStateName(m_tracker.state())},
        {"fingers", m_tracker.fingers()},
        {"x", m_tracker.displacement().x()}, {"y", m_tracker.displacement().y()},
        {"progress", m_tracker.progress()}, {"velocity", m_tracker.velocity()},
        {"releaseDirection", m_tracker.releaseDirection()},
        {"beginCount", static_cast<qint64>(m_begins)}, {"updateCount", static_cast<qint64>(m_updates)},
        {"endCount", static_cast<qint64>(m_ends)}, {"cancelCount", static_cast<qint64>(m_cancels)}};
}

}
