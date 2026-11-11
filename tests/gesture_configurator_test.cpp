// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "configurator.h"
#include "gesture_settings.h"
#include "color_picker.h"
#include <QColorDialog>
#include <QDialog>
#include <QTimer>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace NaturalSwipe;

class GestureConfiguratorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void colorPickerHslRgbAndCancel()
    {
        QTimer::singleShot(0, []() {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto picker = dialog->findChild<QColorDialog *>();
            auto hue = dialog->findChild<QDoubleSpinBox *>("hslHue");
            auto saturation = dialog->findChild<QDoubleSpinBox *>("hslSaturation");
            auto lightness = dialog->findChild<QDoubleSpinBox *>("hslLightness");
            QVERIFY(picker && hue && saturation && lightness);
            picker->setCurrentColor(QColor("#ff0000"));
            QCOMPARE(lightness->value(), 50.0);
            hue->setValue(120);
            QCOMPARE(picker->currentColor().name(), QString("#00ff00"));
            saturation->setValue(0);
            QCOMPARE(picker->currentColor().name(), QString("#808080"));
            dialog->accept();
        });
        QCOMPARE(selectOverscrollColor(Qt::black, nullptr).name(), QString("#808080"));
        QTimer::singleShot(0, []() {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->reject();
        });
        QVERIFY(!selectOverscrollColor(Qt::red, nullptr).isValid());
    }

    void colorSaveAndWallpaperRetention()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("gestures.json");
        std::unique_ptr<QWidget> widget(createGestureConfigurator(path));
        auto color = widget->findChild<QPushButton *>("overscrollColor");
        QVERIFY(color);
        QCOMPARE(color->text(), QString("#000000"));
        QTimer::singleShot(0, []() {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QColorDialog *>()->setCurrentColor(QColor("#336699"));
            dialog->accept();
        });
        color->click();
        QCOMPARE(color->text(), QString("#336699"));
        QVERIFY(!QFile::exists(path)); // Selection stages only.
        auto background = widget->findChild<QComboBox *>("overscrollBackground");
        background->setCurrentIndex(background->findData("wallpaper"));
        QVERIFY(!color->isEnabled());
        widget->findChild<QPushButton *>("saveGestureSettings")->click();
        GestureSettingsStore store;
        QVERIFY(store.loadFile(path));
        QCOMPARE(store.settings().overscrollColor, QString("#336699"));
        QCOMPARE(store.settings().overscrollBackground, QString("wallpaper"));
        widget->findChild<QPushButton *>("restoreGestureDefaults")->click();
        QCOMPARE(color->text(), QString("#000000"));
        QVERIFY(color->isEnabled());
    }

    void defaultsSaveAndPanelIsolation()
    {
        QTemporaryDir dir;
        const auto panelPath = dir.filePath("panels.json");
        QFile panels(panelPath);
        QVERIFY(panels.open(QIODevice::WriteOnly));
        const QByteArray original = R"({"schemaVersion":1,"rules":[]})";
        panels.write(original);
        panels.close();
        std::unique_ptr<QWidget> widget(createPanelConfigurator(panelPath));
        auto tabs = widget->findChild<QTabWidget *>("settingsTabs");
        QVERIFY(tabs && tabs->count() == 2);
        auto threshold = widget->findChild<QDoubleSpinBox *>("slowSwipeThreshold");
        auto limit = widget->findChild<QDoubleSpinBox *>("overscrollLimit");
        auto flick = widget->findChild<QDoubleSpinBox *>("flickSettleSpeed");
        auto background = widget->findChild<QComboBox *>("overscrollBackground");
        auto save = widget->findChild<QPushButton *>("saveGestureSettings");
        QVERIFY(threshold && limit && flick && background && save);
        QCOMPARE(threshold->value(), 30.0);
        QCOMPARE(limit->value(), 10.0);
        QCOMPARE(flick->value(), 1.0);
        QCOMPARE(background->currentData().toString(), QString("black"));
        QVERIFY(!QFile::exists(dir.filePath("gestures.json")));
        threshold->setValue(40);
        limit->setValue(25);
        flick->setValue(2);
        background->setCurrentIndex(background->findData("wallpaper"));
        save->click();
        GestureSettingsStore store;
        QVERIFY(store.loadFile(dir.filePath("gestures.json")));
        QCOMPARE(store.settings().slowSwipeThreshold, 0.4);
        QCOMPARE(store.settings().overscrollLimit, 0.25);
        QCOMPARE(store.settings().flickSettleSpeed, 2.0);
        QCOMPARE(store.settings().overscrollBackground, QString("wallpaper"));
        QVERIFY(panels.open(QIODevice::ReadOnly));
        QCOMPARE(panels.readAll(), original);
        auto defaults = widget->findChild<QPushButton *>("restoreGestureDefaults");
        defaults->click();
        QCOMPARE(threshold->value(), 30.0);
        QCOMPARE(limit->value(), 10.0);
        QVERIFY(store.loadFile(dir.filePath("gestures.json")));
        QCOMPARE(store.settings().slowSwipeThreshold, 0.4); // Restore stages only.
    }

    void invalidSavedFileIsNotOverwritten()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("gestures.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("invalid user data");
        file.close();
        std::unique_ptr<QWidget> widget(createGestureConfigurator(path));
        auto save = widget->findChild<QPushButton *>("saveGestureSettings");
        QVERIFY(save && !save->isEnabled());
        widget->findChild<QPushButton *>("restoreGestureDefaults")->click();
        QVERIFY(!save->isEnabled());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("invalid user data"));
    }

    void statusGrowthKeepsControlsStable_data()
    {
        QTest::addColumn<QSize>("size");
        QTest::newRow("small") << QSize(540, 360);
        QTest::newRow("normal") << QSize(720, 620);
        QTest::newRow("tall") << QSize(720, 1000);
    }

    void statusGrowthKeepsControlsStable()
    {
        QFETCH(QSize, size);
        QTemporaryDir dir;
        std::unique_ptr<QWidget> widget(createGestureConfigurator(dir.filePath("gestures.json")));
        widget->resize(size);
        widget->show();
        QVERIFY(QTest::qWaitForWindowExposed(widget.get()));
        auto status = widget->findChild<QLabel *>("gestureStatus");
        auto save = widget->findChild<QPushButton *>("saveGestureSettings");
        auto scroll = widget->findChild<QScrollArea *>("gestureScroll");
        status->setText("Ready");
        QCoreApplication::processEvents();
        const auto position = save->mapTo(widget.get(), QPoint());
        const int maximum = scroll->verticalScrollBar()->maximum();
        status->setText(QString("Saved settings and backup.\n").repeated(50));
        QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > maximum);
        QCOMPARE(save->mapTo(widget.get(), QPoint()), position);
    }
};
QTEST_MAIN(GestureConfiguratorTest)
#include "gesture_configurator_test.moc"
