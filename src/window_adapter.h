// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "panel_policy.h"

namespace NaturalSwipe
{
// Inputs are global logical coordinates; output is output-local logical geometry.
PanelInfo panelFromWindow(bool isDock, const QString &resourceClass,
                          const QString &output, const QRectF &frame,
                          const QRectF &outputGeometry);
}
