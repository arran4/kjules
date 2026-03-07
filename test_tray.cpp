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
  QMenu* m = tray.contextMenu();
  m->addAction("Test");
  tray.setStandardActionsEnabled(true);
  QTimer::singleShot(500, &app, &QApplication::quit);
  return app.exec();
}
