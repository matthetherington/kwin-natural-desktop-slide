// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QToolButton;

namespace NaturalSwipe
{
struct ReloadFeedback
{
    bool success = false;
    bool deferred = false;
    QString message;
};

// Diagnostic replies are untrusted: malformed/missing fields must not imply success.
ReloadFeedback summarizeSettingsReload(const QString &json, bool gestures);

class SettingsFeedback : public QWidget
{
public:
    explicit SettingsFeedback(const QString &statusName, QWidget *parent = nullptr);
    void setText(const QString &message, const QString &details = {});

private:
    QLabel *m_message;
    QToolButton *m_toggle;
    QPlainTextEdit *m_details;
};
}
