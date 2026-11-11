// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "configurator.h"
#include "config.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QWidget>
#include <memory>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // The picker temporarily hides its only window while waiting for KWin.
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("natural-swipe-config");
    QCommandLineParser parser;
    parser.setApplicationDescription("Click a Plasma panel to configure selective desktop sliding. Never enables an effect automatically.");
    parser.addHelpOption();
    parser.addOption({{"c", "config"}, "Configuration file to edit.", "path",
        NaturalSwipe::panelConfigPath()});
    parser.process(app);
    std::unique_ptr<QWidget> window(NaturalSwipe::createPanelConfigurator(parser.value("config")));
    QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app, &QCoreApplication::quit);
    window->show();
    return app.exec();
}
