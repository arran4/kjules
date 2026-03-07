#include <QApplication>
#include <KStatusNotifierItem>
#include <QMenu>
#include <QDebug>
#include <QAction>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    KStatusNotifierItem *tray = new KStatusNotifierItem(nullptr);
    tray->setStatus(KStatusNotifierItem::Active);
    QMenu *menu = tray->contextMenu();
    QAction* a1 = menu->addAction("Test 1");
    qDebug() << "Menu items count:" << menu->actions().count();

    QAction* a2 = new QAction("Test 2");
    tray->addAction("test2", a2);

    qDebug() << "Menu items count after addAction:" << tray->contextMenu()->actions().count();

    for (auto action : tray->contextMenu()->actions()) {
        qDebug() << "Action text:" << action->text();
    }
}
