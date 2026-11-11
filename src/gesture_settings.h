// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace NaturalSwipe
{
struct GestureSettings
{
    double slowSwipeThreshold = 0.3;
    double distancePerDesktop = 300;
    double flickVelocity = 1;
    bool overscrollEnabled = true;
    double overscrollLimit = 0.2;
    QString overscrollBackground = QStringLiteral("black");
    QString overscrollColor = QStringLiteral("#000000");
    double settleSpeed = 1;
    double flickSettleSpeed = 1;
    double returnSpeed = 1;
    bool followups = true;
    int edgeGraceMs = 400;
    bool operator==(const GestureSettings &) const = default;
};

QString gestureConfigPath();
bool validGestureSettings(const GestureSettings &, QString *error = nullptr);
QJsonObject gestureSettingsJson(const GestureSettings &);

class GestureSettingsStore
{
public:
    bool reload(const QByteArray &, QString *error = nullptr);
    bool loadFile(const QString &, QString *error = nullptr);
    const GestureSettings &settings() const { return m_settings; }
private:
    GestureSettings m_settings;
};

bool saveGestureSettings(const QString &path, bool existed, const QByteArray &original,
                         const GestureSettings &, QString *backupPath, QString *error);
}
