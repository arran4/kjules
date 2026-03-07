#include <QApplication>
#include <KStatusNotifierItem>
#include <QMenu>
#include <QDebug>
#include <QAction>
#include <QMainWindow>
#include <QTimer>

class TestApp : public QMainWindow {
    Q_OBJECT
public:
    TestApp() {
        KStatusNotifierItem *tray = new KStatusNotifierItem(this);
        tray->setStatus(KStatusNotifierItem::Active);
        QMenu *menu = tray->contextMenu();
        menu->addAction("Test");
        tray->setAssociatedWidget(this);
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TestApp window;
    QTimer::singleShot(100, &app, &QApplication::quit);
    app.exec();
}

#include "test_tray4.moc"
