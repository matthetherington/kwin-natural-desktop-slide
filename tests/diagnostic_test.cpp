// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

class DiagnosticTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void validConfiguration()
    {
        QProcess process;
        process.start(QStringLiteral(DIAGNOSTIC_PATH), {"--config", QStringLiteral(EXAMPLES_PATH "/panels.json")});
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 0);
        QVERIFY(process.readAllStandardError().isEmpty());
        QVERIFY(process.readAllStandardOutput().contains("Configuration valid: 2 rules"));
    }

    void invalidUsage()
    {
        QProcess process;
        // Reject arguments before attempting live discovery or contacting D-Bus.
        process.start(QStringLiteral(DIAGNOSTIC_PATH), {"--discover", "unexpected"});
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 2);
        QVERIFY(process.readAllStandardOutput().isEmpty());
        QVERIFY(process.readAllStandardError().contains("Unexpected positional arguments"));
    }

    void snapshotDecisions()
    {
        QProcess process;
        process.start(QStringLiteral(DIAGNOSTIC_PATH), {"--config", QStringLiteral(EXAMPLES_PATH "/panels.json"),
                      "--snapshot", QStringLiteral(EXAMPLES_PATH "/panel-snapshot.json")});
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        QVERIFY(process.readAllStandardError().isEmpty());
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(process.readAllStandardOutput(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        QVERIFY(document.isArray());
        const auto results = document.array();
        QCOMPARE(results.size(), 4);
        QCOMPARE(results[0].toObject()["action"].toString(), QString("slide"));
        QCOMPARE(results[0].toObject()["ruleIndex"].toInt(), 0);
        QCOMPARE(results[1].toObject()["action"].toString(), QString("slide"));
        QCOMPARE(results[1].toObject()["ruleIndex"].toInt(), 1);
        QCOMPARE(results[2].toObject()["action"].toString(), QString("fixed"));
        QVERIFY(results[2].toObject()["ruleIndex"].isNull());
        QCOMPARE(results[3].toObject()["action"].toString(), QString("fixed"));
    }

    void invalidSnapshotProducesNoPartialResults()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath("invalid.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray data = R"([
            {"label":"valid","isPlasmaPanel":true,"output":"eDP-1",
             "geometry":[12,100,48,800],"outputGeometry":[0,0,1920,1080]},
            {"label":"invalid"}])";
        QCOMPARE(file.write(data), data.size());
        file.close();
        QProcess process;
        process.start(QStringLiteral(DIAGNOSTIC_PATH), {"--config", QStringLiteral(EXAMPLES_PATH "/panels.json"),
                      "--snapshot", file.fileName()});
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 1);
        QVERIFY(process.readAllStandardOutput().isEmpty());
        QVERIFY(process.readAllStandardError().contains("snapshot[1]"));
    }

    void invalidConfiguration()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath("invalid.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("{}") > 0);
        file.close();
        QProcess process;
        process.start(QStringLiteral(DIAGNOSTIC_PATH), {"--config", file.fileName()});
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 1);
        QVERIFY(process.readAllStandardOutput().isEmpty());
        QVERIFY(process.readAllStandardError().contains("schemaVersion"));
    }
};

QTEST_GUILESS_MAIN(DiagnosticTest)
#include "diagnostic_test.moc"
