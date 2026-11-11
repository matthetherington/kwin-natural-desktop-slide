// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "config.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTextStream>

#include <cmath>

using namespace NaturalSwipe;

namespace
{
bool rect(const QJsonValue &value, QRectF &result)
{
    if (!value.isArray()) return false;
    const auto a = value.toArray();
    if (a.size() != 4) return false;
    for (const auto &v : a) {
        if (!v.isDouble() || !std::isfinite(v.toDouble())) return false;
    }
    result = {a[0].toDouble(), a[1].toDouble(), a[2].toDouble(), a[3].toDouble()};
    return result.width() > 0 && result.height() > 0
        && std::isfinite(result.right()) && std::isfinite(result.bottom());
}

int discover(QTextStream &out, QTextStream &err)
{
    QDBusInterface plasma("org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell");
    // This fixed script only reads properties. Never accept arbitrary script input.
    const QString script = QStringLiteral(
        "print(JSON.stringify(panels().map(function(p) { return {id:p.id, screen:p.screen, "
        "location:p.location, height:p.height, length:p.length, offset:p.offset, "
        "alignment:p.alignment, hiding:p.hiding, floating:p.floating}; })))");
    QDBusReply<QString> reply = plasma.call("evaluateScript", script);
    if (!reply.isValid()) {
        err << "Plasma discovery failed: " << reply.error().message() << '\n';
        return 1;
    }
    QJsonParseError parseError;
    const auto panels = QJsonDocument::fromJson(reply.value().trimmed().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !panels.isArray()) {
        err << "Plasma returned unexpected metadata (expected a JSON panel array).\n";
        return 1;
    }
    QProcess process;
    process.start("kscreen-doctor", {"-j"});
    if (!process.waitForStarted(5000) || !process.waitForFinished(5000)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        err << "Output discovery requires a working kscreen-doctor -j.\n";
        return 1;
    }
    const auto outputs = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !outputs.isObject()) {
        err << "kscreen-doctor returned unexpected metadata.\n";
        return 1;
    }
    QJsonObject result;
    result["plasmaPanels"] = panels.array();
    result["displayMetadata"] = outputs.object();
    result["note"] = "Read-only discovery, not a window snapshot. Plasma screen indices are not "
                     "KScreen output IDs. Exact frame geometry and KWin window identity require "
                     "the effect's panelSnapshot() method; do not infer matches from thickness/length alone.";
    out << QJsonDocument(result).toJson();
    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("natural-swipe-diagnostic");
    QCommandLineParser parser;
    parser.setApplicationDescription("Read-only panel discovery, config validation and snapshot rule evaluation.");
    parser.addHelpOption();
    parser.addOptions({{{"c", "config"}, "Validate a configuration file.", "path"},
                       {{"s", "snapshot"}, "Evaluate a panel snapshot (requires --config).", "path"},
                       {"discover", "Read live Plasma panel and KScreen display metadata; no settings changed."}});
    parser.process(app);
    QTextStream out(stdout), err(stderr);
    if (!parser.positionalArguments().isEmpty()) {
        err << "Unexpected positional arguments. Use --help for usage.\n";
        return 2;
    }
    if (parser.isSet("discover")) {
        if (parser.isSet("config") || parser.isSet("snapshot")) {
            err << "--discover cannot be combined with --config or --snapshot.\n";
            return 2;
        }
        return discover(out, err);
    }
    if (!parser.isSet("config")) parser.showHelp(2);
    ConfigStore config;
    QString error;
    if (!config.loadFile(parser.value("config"), &error)) {
        err << error << '\n';
        return 1;
    }
    if (!parser.isSet("snapshot")) {
        out << "Configuration valid: " << config.rules().size() << " rules; default fixed.\n";
        return 0;
    }
    QFile file(parser.value("snapshot"));
    if (!file.open(QIODevice::ReadOnly)) {
        err << file.errorString() << '\n';
        return 1;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        err << "snapshot: expected a JSON array.\n";
        return 1;
    }
    QJsonArray results;
    const auto snapshot = document.array();
    for (qsizetype i = 0; i < snapshot.size(); ++i) {
        const auto object = snapshot[i].toObject();
        PanelInfo panel;
        if (!snapshot[i].isObject() || !object["label"].isString()
            || !object["isPlasmaPanel"].isBool() || !object["output"].isString()
            || object["output"].toString().isEmpty()
            || !rect(object["geometry"], panel.geometry)
            || !rect(object["outputGeometry"], panel.outputGeometry)
            || panel.outputGeometry.topLeft() != QPointF(0, 0)) {
            err << "snapshot[" << i << "]: requires label, isPlasmaPanel, output, geometry "
                << "and outputGeometry; rectangles are [x,y,width,height], output-local.\n";
            return 1;
        }
        panel.output = object["output"].toString();
        panel.isPlasmaPanel = object["isPlasmaPanel"].toBool();
        const auto decision = selectPanel(panel, config.rules());
        QJsonObject result;
        result["label"] = object["label"];
        result["output"] = panel.output;
        result["action"] = decision.action == PanelAction::Slide ? "slide" : "fixed";
        if (decision.ruleIndex) {
            result["ruleIndex"] = static_cast<qint64>(*decision.ruleIndex);
            result["ruleName"] = config.rules()[*decision.ruleIndex].name;
        } else {
            result["ruleIndex"] = QJsonValue::Null;
            result["reason"] = "No eligible matching rule (default fixed).";
        }
        results.append(result);
    }
    out << QJsonDocument(results).toJson();
    return 0;
}
