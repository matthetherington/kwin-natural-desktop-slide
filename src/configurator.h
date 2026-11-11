// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>

class QWidget;

namespace NaturalSwipe
{
QWidget *createPanelConfigurator(const QString &path);
QWidget *createGestureConfigurator(const QString &path);
}
