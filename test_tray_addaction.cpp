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

  QAction* a1 = new QAction("Test 1");
  QAction* a2 = new QAction("Test 2");

  tray.contextMenu()->addAction(a1);
  tray.contextMenu()->addAction(a2);

  QTimer::singleShot(500, &app, &QApplication::quit);
  return app.exec();
}
