#include "activitylogwindow.h"

#include <KI18n/KLocalizedString>
#include <QCloseEvent>
#include <QDateTime>
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

  setupGUI(Default, QString());
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
