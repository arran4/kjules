#include <QApplication>
#include <KStatusNotifierItem>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QTimer>
int main(int argc, char** argv) {
  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  KStatusNotifierItem tray(nullptr);
  tray.setCategory(KStatusNotifierItem::ApplicationStatus);
  tray.setStatus(KStatusNotifierItem::Active);

  // Set context menu
  QMenu* menu = new QMenu();
  menu->addAction("Test 1");
  menu->addAction("Test 2");
  tray.setContextMenu(menu);

  QTimer::singleShot(500, &app, &QApplication::quit);
  return app.exec();
}
