#include <QApplication>
#include <KStatusNotifierItem>
#include <QMenu>
#include <QDebug>
#include <QAction>
#include <QMainWindow>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QMainWindow window;
    KStatusNotifierItem *tray = new KStatusNotifierItem(&window);
    tray->setStatus(KStatusNotifierItem::Active);
    QMenu *menu = tray->contextMenu();
    menu->addAction("Test");

    // Setting associated widget AFTER adding actions to context menu!
    tray->setAssociatedWidget(&window);

    qDebug() << "Menu items count after setAssociatedWidget:" << tray->contextMenu()->actions().count();

    for (auto action : tray->contextMenu()->actions()) {
        qDebug() << "Action text:" << action->text();
    }
}
