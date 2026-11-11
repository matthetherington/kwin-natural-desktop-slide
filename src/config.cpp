// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "config.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>
#include <QStandardPaths>

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

bool knownKeys(const QJsonObject &object, const QStringList &allowed,
               const QString &path, QString *error)
{
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!allowed.contains(it.key())) {
            return fail(error, path + ": unknown property '" + it.key() + "'");
        }
    }
    return true;
}

bool number(const QJsonValue &value)
{
    return value.isDouble() && std::isfinite(value.toDouble());
}

bool parseRule(const QJsonObject &object, PanelRule &rule, const QString &path, QString *error)
{
    if (!knownKeys(object, {"name", "output", "edge", "maxInset", "geometryBounds", "action"}, path, error)) {
        return false;
    }
    for (const auto &key : {QStringLiteral("name"), QStringLiteral("output")}) {
        if (object.contains(key) && (!object[key].isString() || object[key].toString().trimmed().isEmpty())) {
            return fail(error, path + "." + key + ": expected a non-empty string");
        }
    }
    rule.name = object["name"].toString();
    if (object.contains("output")) rule.output = object["output"].toString();
    if (object.contains("edge")) {
        const auto value = object["edge"].toString();
        if (value == "left") rule.edge = Edge::Left;
        else if (value == "right") rule.edge = Edge::Right;
        else if (value == "top") rule.edge = Edge::Top;
        else if (value == "bottom") rule.edge = Edge::Bottom;
        else return fail(error, path + ".edge: expected left, right, top or bottom");
    }
    if (object.contains("maxInset")) {
        if (!number(object["maxInset"]) || object["maxInset"].toDouble() < 0) {
            return fail(error, path + ".maxInset: expected a finite non-negative number");
        }
        if (!rule.edge) return fail(error, path + ".maxInset: requires edge");
        rule.maxInset = object["maxInset"].toDouble();
    }
    if (object.contains("geometryBounds")) {
        if (!object["geometryBounds"].isArray()) {
            return fail(error, path + ".geometryBounds: expected [x, y, width, height]");
        }
        const auto bounds = object["geometryBounds"].toArray();
        if (bounds.size() != 4 || !number(bounds[0]) || !number(bounds[1])
            || !number(bounds[2]) || !number(bounds[3])
            || bounds[2].toDouble() <= 0 || bounds[3].toDouble() <= 0
            || !std::isfinite(bounds[0].toDouble() + bounds[2].toDouble())
            || !std::isfinite(bounds[1].toDouble() + bounds[3].toDouble())) {
            return fail(error, path + ".geometryBounds: expected finite [x, y, positive width, positive height]");
        }
        rule.geometryBounds = QRectF(bounds[0].toDouble(), bounds[1].toDouble(),
                                     bounds[2].toDouble(), bounds[3].toDouble());
    }
    const auto action = object["action"].toString();
    if (action == "slide") rule.action = PanelAction::Slide;
    else if (action == "fixed") rule.action = PanelAction::Fixed;
    else return fail(error, path + ".action: expected slide or fixed");
    return true;
}
}

QString panelConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/kwin-natural-swipe/panels.json");
}

bool ConfigStore::reload(const QByteArray &json, QString *error)
{
    if (error) error->clear();
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return fail(error, QStringLiteral("JSON at byte %1: %2").arg(parseError.offset).arg(parseError.errorString()));
    }
    if (!document.isObject()) return fail(error, "config: expected an object");
    const auto root = document.object();
    if (!knownKeys(root, {"schemaVersion", "defaultAction", "rules"}, "config", error)) return false;
    if (!number(root["schemaVersion"]) || root["schemaVersion"].toDouble() != 1) {
        return fail(error, "config.schemaVersion: expected 1");
    }
    if (root.contains("defaultAction") && root["defaultAction"].toString() != "fixed") {
        return fail(error, "config.defaultAction: only fixed is supported");
    }
    if (!root["rules"].isArray()) return fail(error, "config.rules: expected an array");
    QVector<PanelRule> pending;
    const auto rules = root["rules"].toArray();
    for (qsizetype i = 0; i < rules.size(); ++i) {
        const auto path = QStringLiteral("config.rules[%1]").arg(i);
        if (!rules[i].isObject()) return fail(error, path + ": expected an object");
        PanelRule rule;
        if (!parseRule(rules[i].toObject(), rule, path, error)) return false;
        pending.push_back(rule);
    }
    m_rules = std::move(pending);
    return true;
}

bool ConfigStore::loadFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, path + ": " + file.errorString());
    const auto data = file.readAll();
    if (file.error() != QFileDevice::NoError) return fail(error, path + ": " + file.errorString());
    return reload(data, error);
}

} // namespace NaturalSwipe
