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
        QAction* testAction = new QAction("Test Action");
        menu->addAction(testAction);

        tray->setAssociatedWidget(this);

        qDebug() << "Context menu actions count:" << tray->contextMenu()->actions().count();
        for (auto action : tray->contextMenu()->actions()) {
            qDebug() << "Action text:" << action->text();
        }
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TestApp window;
    QTimer::singleShot(100, &app, &QApplication::quit);
    app.exec();
}

#include "test_tray7.moc"
