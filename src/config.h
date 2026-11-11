// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "panel_policy.h"

#include <QByteArray>

namespace NaturalSwipe
{

QString panelConfigPath();

// Parse completely before replacing active rules. Failure preserves the last
// valid configuration; an unconfigured store has fixed-by-default behaviour.
class ConfigStore
{
public:
    bool reload(const QByteArray &json, QString *error = nullptr);
    bool loadFile(const QString &path, QString *error = nullptr);
    const QVector<PanelRule> &rules() const { return m_rules; }

private:
    QVector<PanelRule> m_rules;
};

} // namespace NaturalSwipe
