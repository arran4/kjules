#include "activitylogwindow.h"

#include <KLocalizedString>
#include <QAction>
#include <QCloseEvent>
#include <QDateTime>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QTextBrowser>

ActivityLogWindow *ActivityLogWindow::instance() {
  static ActivityLogWindow *s_instance = nullptr;
  if (!s_instance) {
    s_instance = new ActivityLogWindow();
  }
  return s_instance;
}

ActivityLogWindow::ActivityLogWindow(QWidget *parent) : KXmlGuiWindow(parent) {
  setObjectName(QStringLiteral("ActivityLogWindow"));
  setWindowTitle(i18n("Activity Log"));

  m_logBrowser = new QTextBrowser(this);
  setCentralWidget(m_logBrowser);

  QMenu *fileMenu = new QMenu(i18n("File"), this);
  QAction *closeAction = new QAction(
      QIcon::fromTheme(QStringLiteral("window-close")), i18n("Close"), this);
  closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
  connect(closeAction, &QAction::triggered, this, &KXmlGuiWindow::close);
  fileMenu->addAction(closeAction);

  menuBar()->addMenu(fileMenu);

  resize(600, 400);
}

ActivityLogWindow::~ActivityLogWindow() = default;

void ActivityLogWindow::logMessage(const QString &message) {
  QString timeStr =
      QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
  m_logBrowser->append(QStringLiteral("[%1] %2").arg(timeStr, message));
}

void ActivityLogWindow::closeEvent(QCloseEvent *event) {
  hide();
  event->ignore();
}
