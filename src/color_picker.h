// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QColor>

class QWidget;
namespace NaturalSwipe
{
// Returns an invalid colour on cancellation. No settings are saved here.
QColor selectOverscrollColor(const QColor &initial, QWidget *parent);
}
