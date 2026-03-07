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

  QMenu* m = new QMenu();
  m->addAction("Test 1");
  m->addAction("Test 2");
  tray.setContextMenu(m);

  QTimer::singleShot(500, &app, &QApplication::quit);
  return app.exec();
}
