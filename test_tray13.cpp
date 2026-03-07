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
        tray->addAction("test_action", testAction);

        tray->setAssociatedWidget(this);

        qDebug() << "Tray actionCollection actions count:" << tray->actionCollection().count();
        for (auto action : tray->actionCollection()) {
            qDebug() << "Action text in collection:" << action->text();
        }
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TestApp window;
    QTimer::singleShot(100, &app, &QApplication::quit);
    app.exec();
}

#include "test_tray13.moc"
