// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings_feedback.h"
#include <QLabel>
#include <QPlainTextEdit>
#include <QTest>
#include <QToolButton>

using namespace NaturalSwipe;

class SettingsFeedbackTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void diagnosticSummaries_data()
    {
        QTest::addColumn<QString>("json");
        QTest::addColumn<bool>("gestures");
        QTest::addColumn<bool>("success");
        QTest::addColumn<bool>("deferred");
        const auto add = [](const char *name, const char *json, bool gestures, bool success, bool deferred) {
            QTest::newRow(name) << QString::fromUtf8(json) << gestures << success << deferred;
        };
        add("panels", R"({"error":"","ruleCount":2,"path":"/private/panels.json"})", false, true, false);
        add("gestures", R"({"gestureError":"","settingsDeferred":false})", true, true, false);
        add("queued", R"({"gestureError":"","settingsDeferred":true})", true, true, true);
        add("panel-error", R"({"error":"private technical failure"})", false, false, false);
        add("gesture-error", R"({"gestureError":"private technical failure"})", true, false, false);
        add("partial-error", R"({"error":"","gestureError":"private technical failure"})", false, false, false);
        add("malformed", "not JSON", false, false, false);
        add("array", "[]", false, false, false);
        add("missing-fields", "{}", true, false, false);
        add("wrong-error-type", R"({"error":false})", false, false, false);
        add("wrong-deferred-type", R"({"error":"","settingsDeferred":"false"})", false, false, false);
    }

    void diagnosticSummaries()
    {
        QFETCH(QString, json);
        QFETCH(bool, gestures);
        QFETCH(bool, success);
        QFETCH(bool, deferred);
        const auto feedback = summarizeSettingsReload(json, gestures);
        QCOMPARE(feedback.success, success);
        QCOMPARE(feedback.deferred, deferred);
        QVERIFY(!feedback.message.isEmpty());
        QVERIFY(!feedback.message.contains("/private"));
        QVERIFY(!feedback.message.contains("private technical failure"));
        QVERIFY(!feedback.message.contains('{'));
    }

    void detailsAreOptionalAndResetForEachAction()
    {
        SettingsFeedback feedback("status");
        feedback.setText("Panel choices applied.", R"({"path":"/private/panels.json"})");
        auto message = feedback.findChild<QLabel *>("status");
        auto toggle = feedback.findChild<QToolButton *>("statusDetailsToggle");
        auto details = feedback.findChild<QPlainTextEdit *>("statusDetails");
        QVERIFY(message && toggle && details);
        QCOMPARE(message->text(), QString("Panel choices applied."));
        QCOMPARE(message->textFormat(), Qt::PlainText);
        QVERIFY(details->isReadOnly());
        QVERIFY(details->isHidden());
        QVERIFY(!toggle->isHidden());
        toggle->click();
        QVERIFY(!details->isHidden());
        QVERIFY(details->toPlainText().contains("/private/panels.json"));
        feedback.setText("Unsaved changes.");
        QVERIFY(details->isHidden());
        QVERIFY(toggle->isHidden());
        QVERIFY(details->toPlainText().isEmpty());
        QVERIFY(!toggle->isChecked());
    }
};

QTEST_MAIN(SettingsFeedbackTest)
#include "settings_feedback_test.moc"
