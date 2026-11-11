// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "gesture_settings.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>
#include <cmath>

namespace NaturalSwipe
{
namespace
{
bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}
struct NumericField { const char *name; double GestureSettings::*member; double min; double max; };
constexpr NumericField numericFields[] = {
    {"slowSwipeThreshold", &GestureSettings::slowSwipeThreshold, 0.05, 0.95},
    {"distancePerDesktop", &GestureSettings::distancePerDesktop, 50, 1500},
    {"flickVelocity", &GestureSettings::flickVelocity, 0.1, 20},
    {"overscrollLimit", &GestureSettings::overscrollLimit, 0.01, 0.5},
    {"settleSpeed", &GestureSettings::settleSpeed, 0.1, 4},
    {"flickSettleSpeed", &GestureSettings::flickSettleSpeed, 0.1, 4},
    {"returnSpeed", &GestureSettings::returnSpeed, 0.1, 4},
};
}

QString gestureConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/kwin-natural-swipe/gestures.json");
}

bool validGestureSettings(const GestureSettings &settings, QString *error)
{
    if (error) error->clear();
    for (const auto &field : numericFields) {
        const double value = settings.*field.member;
        if (!std::isfinite(value) || value < field.min || value > field.max)
            return fail(error, QString("gestures.%1: expected %2 to %3").arg(field.name).arg(field.min).arg(field.max));
    }
    if (settings.overscrollBackground != "black" && settings.overscrollBackground != "wallpaper")
        return fail(error, "gestures.overscrollBackground: expected black or wallpaper");
    static const QRegularExpression colorPattern(QStringLiteral("\\A#[0-9a-fA-F]{6}\\z"));
    if (!colorPattern.match(settings.overscrollColor).hasMatch())
        return fail(error, "gestures.overscrollColor: expected an opaque #RRGGBB colour");
    if (settings.edgeGraceMs < 0 || settings.edgeGraceMs > 1500)
        return fail(error, "gestures.edgeGraceMs: expected an integer from 0 to 1500");
    return true;
}

QJsonObject gestureSettingsJson(const GestureSettings &settings)
{
    QJsonObject json{{"schemaVersion", 1}, {"overscrollEnabled", settings.overscrollEnabled},
        {"overscrollBackground", settings.overscrollBackground}, {"followups", settings.followups},
        {"edgeGraceMs", settings.edgeGraceMs}, {"overscrollColor", settings.overscrollColor}};
    for (const auto &field : numericFields) json[field.name] = settings.*field.member;
    return json;
}

bool GestureSettingsStore::reload(const QByteArray &bytes, QString *error)
{
    if (error) error->clear();
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, "gestures: expected a valid JSON object: " + parse.errorString());
    const auto json = document.object();
    if (!json["schemaVersion"].isDouble() || json["schemaVersion"].toDouble() != 1)
        return fail(error, "gestures.schemaVersion: expected 1");
    GestureSettings pending;
    const auto defaults = gestureSettingsJson(pending);
    for (auto it = json.begin(); it != json.end(); ++it) {
        if (!defaults.contains(it.key())) return fail(error, "gestures: unknown property '" + it.key() + "'");
    }
    for (const auto &field : numericFields) {
        if (!json.contains(field.name)) continue;
        if (!json[field.name].isDouble()) return fail(error, QString("gestures.%1: expected a number").arg(field.name));
        pending.*field.member = json[field.name].toDouble();
    }
    for (const auto &key : {"overscrollEnabled", "followups"}) {
        if (json.contains(key) && !json[key].isBool()) return fail(error, QString("gestures.%1: expected a boolean").arg(key));
    }
    pending.overscrollEnabled = json["overscrollEnabled"].toBool(pending.overscrollEnabled);
    pending.followups = json["followups"].toBool(pending.followups);
    if (json.contains("overscrollColor")) {
        if (!json["overscrollColor"].isString()) return fail(error, "gestures.overscrollColor: expected a string");
        pending.overscrollColor = json["overscrollColor"].toString();
    }
    if (json.contains("overscrollBackground")) {
        if (!json["overscrollBackground"].isString()) return fail(error, "gestures.overscrollBackground: expected a string");
        pending.overscrollBackground = json["overscrollBackground"].toString();
    }
    if (json.contains("edgeGraceMs")) {
        const auto value = json["edgeGraceMs"];
        if (!value.isDouble() || !std::isfinite(value.toDouble()) || value.toDouble() < 0
            || value.toDouble() > 1500 || std::floor(value.toDouble()) != value.toDouble())
            return fail(error, "gestures.edgeGraceMs: expected an integer from 0 to 1500");
        pending.edgeGraceMs = value.toInt();
    }
    if (!validGestureSettings(pending, error)) return false;
    m_settings = pending;
    return true;
}

bool GestureSettingsStore::loadFile(const QString &path, QString *error)
{
    if (!QFileInfo::exists(path)) {
        m_settings = {};
        if (error) error->clear();
        return true; // Missing file means tuned defaults; do not create it.
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, path + ": " + file.errorString());
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) return fail(error, file.errorString());
    return reload(bytes, error);
}

bool saveGestureSettings(const QString &path, bool existed, const QByteArray &original,
                         const GestureSettings &settings, QString *backupPath, QString *error)
{
    if (backupPath) backupPath->clear();
    if (!validGestureSettings(settings, error)) return false;
    const QFileInfo info(path);
    if (info.isSymLink()) return fail(error, "Refusing to replace a symlink settings file.");
    if (info.exists() != existed) return fail(error, "Settings appeared/disappeared. Reload before saving.");
    if (existed) {
        QFile current(path);
        if (!current.open(QIODevice::ReadOnly)) return fail(error, current.errorString());
        const auto bytes = current.readAll();
        if (current.error() != QFileDevice::NoError || bytes != original)
            return fail(error, "Settings changed in another program. Reload before saving.");
    }
    if (!QDir().mkpath(info.absolutePath())) return fail(error, "Cannot create the settings directory.");
    if (existed) {
        const auto backup = path + ".backup-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!QFile::copy(path, backup)) return fail(error, "Cannot back up settings; nothing saved.");
        if (backupPath) *backupPath = backup;
    }
    const auto bytes = QJsonDocument(gestureSettingsJson(settings)).toJson();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    if (!existed) file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(bytes) != bytes.size() || !file.commit()) return fail(error, file.errorString());
    return true;
}
}
