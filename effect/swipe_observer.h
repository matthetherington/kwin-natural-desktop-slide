// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "swipe_tracker.h"
#include "input_event_spy.h"
#include <QJsonObject>

namespace KWin
{

// Observe only: InputEventSpy cannot consume events. No renderer or shortcut
// callbacks are invoked here. Built/installed registration is opt-in.
class SwipeObserver : public InputEventSpy
{
public:
    void swipeGestureBegin(PointerSwipeGestureBeginEvent *event) override;
    void swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event) override;
    void swipeGestureEnd(PointerSwipeGestureEndEvent *event) override;
    void swipeGestureCancelled(PointerSwipeGestureCancelEvent *event) override;
    void clear();
    QJsonObject status() const;

private:
    NaturalSwipe::SwipeTracker m_tracker;
    quint64 m_begins = 0;
    quint64 m_updates = 0;
    quint64 m_ends = 0;
    quint64 m_cancels = 0;
};

}
