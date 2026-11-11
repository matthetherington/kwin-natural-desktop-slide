// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include <QApplication>
#include <QLabel>
#include <QScreen>
#include <QWindow>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList colors{"#802020", "#208020", "#202080"};
    int index = 0;
    for (auto screen : app.screens()) {
        auto window = new QLabel("Natural Desktop Slide isolated output test");
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->setStyleSheet("background:" + colors[index++ % colors.size()] + ";color:white");
        window->setWindowTitle("Natural Desktop Slide test output");
        window->winId();
        window->windowHandle()->setScreen(screen);
        window->showFullScreen();
    }
    return app.exec();
}
