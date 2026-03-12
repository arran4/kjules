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
#include "settingsdialog.h"
#include "newsessiondialog.h"
#include "sourcemodel.h"
#include "apimanager.h"
#include <QMenuBar>
#include <QMenu>

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
    window.resize(800, 600);

    // In MOCK_UI_TEST, the MainWindow should initialize mock models and disable actions.
    window.show();

    QTimer::singleShot(1000, [&window]() {
        takeScreenshots(&window);

        // Settings Dialog
        APIManager apiManager;
        SettingsDialog settingsDlg(&apiManager, &window);
        settingsDlg.show();
        QApplication::processEvents();
        settingsDlg.grab().save("screenshot_settings.png");
        qDebug() << "Saved screenshot_settings.png";
        settingsDlg.close();

        // New Session Dialog
        SourceModel sourceModel;
        NewSessionDialog sessionDlg(&sourceModel, true, &window);
        sessionDlg.resize(800, 600);
        sessionDlg.show();
        QApplication::processEvents();
        sessionDlg.grab().save("screenshot_newsession.png");
        qDebug() << "Saved screenshot_newsession.png";
        sessionDlg.close();

        // Tray Menu
        if (QMenu* trayMenu = window.getTrayMenu()) {
            trayMenu->show();
            QApplication::processEvents();
            trayMenu->grab().save("screenshot_traymenu.png");
            qDebug() << "Saved screenshot_traymenu.png";
            trayMenu->hide();
        }
    });

    // Run for a while then quit
    QTimer::singleShot(2000, &app, &QApplication::quit);

    return app.exec();
}
