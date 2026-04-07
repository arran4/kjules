#ifndef ACTIVITYLOGWINDOW_H
#define ACTIVITYLOGWINDOW_H

#include <KXmlGuiWindow>

class QTextBrowser;

class ActivityLogWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  static ActivityLogWindow *instance();
  void logMessage(const QString &message);

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  explicit ActivityLogWindow(QWidget *parent = nullptr);
  ~ActivityLogWindow() override;

  QTextBrowser *m_logBrowser;
};

#endif // ACTIVITYLOGWINDOW_H
