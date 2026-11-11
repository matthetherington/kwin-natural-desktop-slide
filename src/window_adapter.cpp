// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "window_adapter.h"

namespace NaturalSwipe
{
PanelInfo panelFromWindow(bool isDock, const QString &resourceClass,
                          const QString &output, const QRectF &frame,
                          const QRectF &outputGeometry)
{
    const bool plasma = resourceClass == QStringLiteral("plasmashell")
        || resourceClass == QStringLiteral("org.kde.plasmashell");
    return {isDock && plasma, output, frame.translated(-outputGeometry.topLeft()),
            QRectF(QPointF(0, 0), outputGeometry.size())};
}
}
