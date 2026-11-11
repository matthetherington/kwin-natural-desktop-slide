// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "panel_policy.h"
#include <QByteArray>
#include <QVariantMap>

namespace NaturalSwipe
{
struct OutputInfo { QString name; QRectF geometry; };
bool pickedPanel(const QVariantMap &window, const QVector<OutputInfo> &outputs,
                 PanelInfo &panel, QString *error);
QString describePanel(const PanelInfo &panel);
bool panelOverride(const QByteArray &original, const PanelInfo &panel,
                   PanelAction action, QByteArray &updated, QString *error);
struct PanelChoice { PanelInfo panel; PanelAction action; };
bool panelOverrides(const QByteArray &original, QVector<PanelChoice> choices,
                    QByteArray &updated, QString *error);
// Optimistic concurrency: do not overwrite edits made since the UI read the file.
// Existing files get a uniquely named backup before atomic replacement.
bool savePanelConfig(const QString &path, bool existed, const QByteArray &original,
                     const QByteArray &updated, QString *backupPath, QString *error);
}
