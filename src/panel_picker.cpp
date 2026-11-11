// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "panel_picker.h"
#include "config.h"
#include "window_adapter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <cmath>
#include <algorithm>

namespace NaturalSwipe
{
namespace
{
bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}
bool validRect(const QRectF &r)
{
    return std::isfinite(r.x()) && std::isfinite(r.y()) && std::isfinite(r.width())
        && std::isfinite(r.height()) && std::isfinite(r.right()) && std::isfinite(r.bottom())
        && r.width() > 0 && r.height() > 0;
}
}

bool pickedPanel(const QVariantMap &window, const QVector<OutputInfo> &outputs,
                 PanelInfo &panel, QString *error)
{
    if (error) error->clear();
    bool typeOk = false;
    // NET::Dock is 2 in KWin's D-Bus window-info protocol.
    const bool dock = window.value("type").toInt(&typeOk) == 2 && typeOk;
    const QString identity = window.value("resourceClass").toString();
    if (!dock || (identity != "plasmashell" && identity != "org.kde.plasmashell")) {
        return fail(error, "That window is not a Plasma panel. Click the panel itself, not a menu or application.");
    }
    double values[4];
    int i = 0;
    for (const auto key : {"x", "y", "width", "height"}) {
        bool ok = false;
        values[i++] = window.value(key).toDouble(&ok);
        if (!ok) return fail(error, "KWin returned incomplete panel geometry.");
    }
    const QRectF frame(values[0], values[1], values[2], values[3]);
    if (!validRect(frame)) return fail(error, "KWin returned invalid panel geometry.");
    std::optional<PanelInfo> candidate;
    for (const auto &output : outputs) {
        if (output.name.isEmpty() || !validRect(output.geometry) || !output.geometry.contains(frame)) continue;
        if (candidate) return fail(error, "Panel belongs to overlapping outputs; cannot choose a safe output match.");
        candidate = panelFromWindow(true, identity, output.name, frame, output.geometry);
    }
    if (!candidate) return fail(error, "Panel frame does not fit a single known output. Check display scaling/layout and try again.");
    panel = *candidate;
    return true;
}

QString describePanel(const PanelInfo &panel)
{
    const auto &r = panel.geometry;
    return QStringLiteral("%1 — position (%2, %3), size %4 × %5 logical pixels")
        .arg(panel.output).arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

bool panelOverride(const QByteArray &original, const PanelInfo &panel,
                   PanelAction action, QByteArray &updated, QString *error)
{
    ConfigStore validator;
    if (!validator.reload(original, error)) return false;
    if (!panel.isPlasmaPanel || panel.output.isEmpty() || !validRect(panel.geometry)
        || !validRect(panel.outputGeometry) || panel.outputGeometry.topLeft() != QPointF()
        || !panel.outputGeometry.contains(panel.geometry)) return fail(error, "No valid panel selected.");
    auto root = QJsonDocument::fromJson(original).object();
    const auto &r = panel.geometry;
    const QJsonArray bounds{r.x(), r.y(), r.width(), r.height()};
    const QString name = QStringLiteral("Picker: ") + describePanel(panel);
    QJsonArray rules;
    rules.append(QJsonObject{{"name", name}, {"output", panel.output}, {"geometryBounds", bounds},
                             {"action", action == PanelAction::Slide ? "slide" : "fixed"}});
    for (const auto &value : root["rules"].toArray()) {
        const auto rule = value.toObject();
        // Only replace our own exact override; leave all other rules intact.
        if (rule.size() == 4 && rule["name"].toString() == name
            && rule["output"].toString() == panel.output && rule["geometryBounds"] == bounds) continue;
        rules.append(value);
    }
    root["rules"] = rules;
    const auto pending = QJsonDocument(root).toJson();
    if (!validator.reload(pending, error)) return false;
    updated = pending;
    return true;
}

bool panelOverrides(const QByteArray &original, QVector<PanelChoice> choices,
                    QByteArray &updated, QString *error)
{
    // Smaller areas must take priority over enclosing panels' bounds overrides.
    std::stable_sort(choices.begin(), choices.end(), [](const auto &a, const auto &b) {
        return a.panel.geometry.width() * a.panel.geometry.height()
            > b.panel.geometry.width() * b.panel.geometry.height();
    });
    ConfigStore validator;
    if (!validator.reload(original, error)) return false;
    QByteArray pending = original;
    for (const auto &choice : choices) {
        QByteArray next;
        if (!panelOverride(pending, choice.panel, choice.action, next, error)) return false;
        pending = next;
    }
    updated = pending;
    return true;
}

bool savePanelConfig(const QString &path, bool existed, const QByteArray &original,
                     const QByteArray &updated, QString *backupPath, QString *error)
{
    if (error) error->clear();
    if (backupPath) backupPath->clear();
    ConfigStore validator;
    if (!validator.reload(updated, error)) return false;
    const QFileInfo info(path);
    if (info.isSymLink()) return fail(error, "Refusing to replace a symlink configuration file.");
    if (info.exists() != existed) return fail(error, "Configuration appeared/disappeared since it was read. Reload saved rules before saving.");
    if (existed) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
        const auto current = file.readAll();
        if (file.error() != QFileDevice::NoError) return fail(error, file.errorString());
        if (current != original) return fail(error, "Configuration changed in another program. Reload saved rules before saving.");
    }
    if (!QDir().mkpath(info.absolutePath())) return fail(error, "Cannot create the configuration directory.");
    if (existed) {
        const auto backup = path + ".backup-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!QFile::copy(path, backup)) return fail(error, "Cannot back up the existing configuration; nothing saved.");
        if (backupPath) *backupPath = backup;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    if (!existed) file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(updated) != updated.size() || !file.commit()) return fail(error, file.errorString());
    return true;
}
}
