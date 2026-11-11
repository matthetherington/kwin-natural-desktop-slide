// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include <KCModule>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTest>
#include <memory>

class SettingsModuleTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void loadEmbeddedPicker()
    {
        // Instantiate the settings plugin, not the compositor effect. Merely
        // opening the UI neither talks to KWin nor reads/writes panel config.
        const KPluginMetaData metadata(QStringLiteral(SETTINGS_MODULE_PATH));
        QVERIFY(metadata.isValid());
        QCOMPARE(metadata.pluginId(), QString("kwin_natural_desktop_slide_config"));
        const auto result = KPluginFactory::instantiatePlugin<KCModule>(metadata);
        QVERIFY2(result.plugin, qPrintable(result.errorString));
        std::unique_ptr<KCModule> module(result.plugin);
        QCOMPARE(module->buttons(), KCModule::Buttons(KCModule::NoAdditionalButton));
        auto host = module->widget();
        QVERIFY(host);
        auto save = host->findChild<QPushButton *>("savePanelChoices");
        auto status = host->findChild<QLabel *>("pickerStatus");
        QVERIFY(save && status);
        QVERIFY(!save->isEnabled());
        QCOMPARE(host->findChildren<QTableWidget *>().size(), 1);
        QVERIFY(save->window() == host->window());
        host->resize(720, 620);
        host->show();
        QVERIFY(QTest::qWaitForWindowExposed(host));
    }
};

QTEST_MAIN(SettingsModuleTest)
#include "settings_module_test.moc"
