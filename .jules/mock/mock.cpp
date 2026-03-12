#include <QApplication>
#include <QTimer>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QScreen>
#include <QPixmap>
#include <QTabWidget>
#include "mainwindow.h"

// Define MOCK_UI_TEST if not defined by compiler
#ifndef MOCK_UI_TEST
#define MOCK_UI_TEST
#endif

void takeScreenshots(MainWindow* window) {
    QList<QTabWidget*> tabs = window->findChildren<QTabWidget*>();
    if (!tabs.isEmpty()) {
        QTabWidget* tabWidget = tabs.first();
        for (int i = 0; i < tabWidget->count(); ++i) {
            tabWidget->setCurrentIndex(i);
            QApplication::processEvents();
            QPixmap pixmap = window->grab();
            QString filename = QString("screenshot_tab_%1.png").arg(i);
            pixmap.save(filename);
            qDebug() << "Saved screenshot:" << filename;
        }
    } else {
        QPixmap pixmap = window->grab();
        pixmap.save("screenshot_main.png");
        qDebug() << "Saved screenshot_main.png";
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Create the window
    MainWindow window;

    // In MOCK_UI_TEST, the MainWindow should initialize mock models and disable actions.
    window.show();

    QTimer::singleShot(1000, [&window]() {
        takeScreenshots(&window);
    });

    // Run for a while then quit
    QTimer::singleShot(2000, &app, &QApplication::quit);

    return app.exec();
}
