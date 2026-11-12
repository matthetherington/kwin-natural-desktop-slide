// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings_feedback.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace NaturalSwipe
{
ReloadFeedback summarizeSettingsReload(const QString &json, bool gestures)
{
    const auto document = QJsonDocument::fromJson(json.toUtf8());
    const auto object = document.object();
    const auto key = gestures ? "gestureError" : "error";
    if (!document.isObject() || !object[key].isString()
        || (object.contains("settingsDeferred") && !object["settingsDeferred"].isBool())) {
        return {false, false, "The effect replied, but its settings could not be verified. See Technical details."};
    }
    if (!object[key].toString().isEmpty()) {
        return {false, false, "The new settings could not be applied. The previous valid settings are still in use. See Technical details."};
    }
    if (!gestures && !object["gestureError"].toString().isEmpty()) {
        return {false, false, "Panel choices applied, but gesture settings could not be applied. See Technical details."};
    }
    if (object["settingsDeferred"].toBool()) {
        return {true, true, "Settings received. They will take effect after the current desktop animation finishes."};
    }
    return {true, false, gestures ? "Gesture and animation settings applied." : "Panel choices applied."};
}

SettingsFeedback::SettingsFeedback(const QString &statusName, QWidget *parent) : QWidget(parent)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_message = new QLabel;
    m_message->setObjectName(statusName);
    m_message->setWordWrap(true);
    m_message->setTextFormat(Qt::PlainText);
    m_message->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_message->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    layout->addWidget(m_message);
    m_toggle = new QToolButton;
    m_toggle->setObjectName(statusName + "DetailsToggle");
    m_toggle->setText("Technical details");
    m_toggle->setCheckable(true);
    m_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toggle->setArrowType(Qt::RightArrow);
    layout->addWidget(m_toggle);
    m_details = new QPlainTextEdit;
    m_details->setObjectName(statusName + "Details");
    m_details->setAccessibleName("Technical details");
    m_details->setReadOnly(true);
    m_details->setFixedHeight(fontMetrics().lineSpacing() * 8);
    layout->addWidget(m_details);
    connect(m_toggle, &QToolButton::toggled, this, [this](bool expanded) {
        m_details->setVisible(expanded);
        m_toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    });
    setText({});
}

void SettingsFeedback::setText(const QString &message, const QString &details)
{
    m_message->setText(message);
    m_toggle->setChecked(false);
    m_toggle->setVisible(!details.isEmpty());
    m_details->setPlainText(details);
    m_details->hide();
}
}
