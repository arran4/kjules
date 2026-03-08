#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QApplication::setQuitOnLastWindowClosed(false);

  QSystemTrayIcon tray;
  tray.setIcon(QIcon::fromTheme("application-x-executable"));

  QMenu *m = new QMenu();
  QAction *a1 = m->addAction("Test");
  QAction *a2 = m->addAction("Quit");
  QObject::connect(a2, &QAction::triggered, &app, &QCoreApplication::quit);

  tray.setContextMenu(m);
  tray.show();

  qDebug() << "menu ptr" << m;
  qDebug() << "actions" << m->actions().size();
  for (auto *a : m->actions()) {
    qDebug() << a->text() << a->isVisible() << a->isEnabled();
  }

  QObject::connect(&tray, &QSystemTrayIcon::activated,
                   [](QSystemTrayIcon::ActivationReason reason) {
                     qDebug() << "tray activated reason =" << reason;
                   });

  return app.exec();
}
