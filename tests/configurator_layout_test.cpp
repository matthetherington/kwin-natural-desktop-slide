// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "configurator.h"
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTest>
#include <memory>

class ConfiguratorLayoutTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void applyingUnsavedPanelChoicesDoesNotContactKWin()
    {
        std::unique_ptr<QWidget> window(NaturalSwipe::createPanelConfigurator("/unused/panels.json"));
        auto remove = window->findChild<QPushButton *>("removePanelChoice");
        auto table = window->findChild<QTableWidget *>("panelChoices");
        auto apply = window->findChild<QPushButton *>("reloadPanelEffect");
        auto status = window->findChild<QLabel *>("pickerStatus");
        QVERIFY(remove && table && apply && status);
        QVERIFY(!remove->isEnabled());
        table->insertRow(0);
        auto choice = new QTableWidgetItem;
        choice->setCheckState(Qt::Checked);
        table->setItem(0, 0, choice);
        apply->click();
        QVERIFY(status->text().contains("Save your panel choices"));
        QVERIFY(!status->text().contains('{'));
    }

    void statusGrowsDownward_data()
    {
        QTest::addColumn<QSize>("windowSize");
        QTest::newRow("normal") << QSize(720, 620);
        QTest::newRow("small") << QSize(540, 360);
        QTest::newRow("tall") << QSize(720, 1000);
    }

    void statusGrowsDownward()
    {
        QFETCH(QSize, windowSize);
        // Constructing this UI does not contact KWin, pick windows or write files.
        std::unique_ptr<QWidget> window(NaturalSwipe::createPanelConfigurator("/unused/panels.json"));
        auto status = window->findChild<QLabel *>("pickerStatus");
        auto save = window->findChild<QPushButton *>("savePanelChoices");
        auto scroll = window->findChild<QScrollArea *>("pickerScroll");
        auto table = window->findChild<QTableWidget *>();
        QVERIFY(status && save && scroll && table);
        status->setText("Ready.");
        window->resize(windowSize);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));
        QCoreApplication::processEvents();
        scroll->verticalScrollBar()->setValue(qMin(100, scroll->verticalScrollBar()->maximum()));
        const QPoint buttonPosition = save->mapTo(window.get(), QPoint());
        const int tableHeight = table->height();
        const int scrollPosition = scroll->verticalScrollBar()->value();
        const int originalHeight = status->height();
        const int originalMaximum = scroll->verticalScrollBar()->maximum();
        const int contentsWidth = scroll->widget()->width();

        status->setText(QString("Saved configuration and backup successfully.\n").repeated(40));
        QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > originalMaximum);
        QVERIFY(status->height() > originalHeight);
        QCOMPARE(save->mapTo(window.get(), QPoint()), buttonPosition);
        QCOMPARE(table->height(), tableHeight);
        QCOMPARE(scroll->verticalScrollBar()->value(), scrollPosition);
        QCOMPARE(scroll->widget()->width(), contentsWidth);

        status->setText("Ready.");
        QTRY_COMPARE(status->height(), originalHeight);
        QCOMPARE(save->mapTo(window.get(), QPoint()), buttonPosition);
        QCOMPARE(table->height(), tableHeight);
    }
};

QTEST_MAIN(ConfiguratorLayoutTest)
#include "configurator_layout_test.moc"
