// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QCoreApplication>
#include <QString>

namespace NaturalSwipe
{
inline bool runtimeMatchesBuild()
{
    // A reviewed source port is not a promise of cross-version binary ABI.
    return QCoreApplication::applicationVersion() == QStringLiteral(NDS_KWIN_BUILD_VERSION);
}
}
