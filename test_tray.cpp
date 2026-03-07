#include <QApplication>
#include <KStatusNotifierItem>
#include <QMenu>
#include <QDebug>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    KStatusNotifierItem *tray = new KStatusNotifierItem(nullptr);
    tray->setStatus(KStatusNotifierItem::Active);
    QMenu *menu = tray->contextMenu();
    menu->addAction("Test");
    qDebug() << "Menu items count:" << menu->actions().count();

    QMenu* menu2 = tray->contextMenu();
    qDebug() << "Menu2 items count:" << menu2->actions().count();

    QMenu *newMenu = new QMenu();
    newMenu->addAction("Test 2");
    tray->setContextMenu(newMenu);

    QMenu* menu3 = tray->contextMenu();
    qDebug() << "Menu3 items count:" << menu3->actions().count();
}
