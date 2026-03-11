#ifndef SESSIONWINDOW_H
#define SESSIONWINDOW_H

#include <KXmlGuiWindow>
#include <QJsonObject>

class QTextBrowser;
class QTabWidget;
class QLabel;
class QTimer;
class QComboBox;
class APIManager;

class SessionWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit SessionWindow(const QJsonObject &sessionData, APIManager *apiManager,
                         QWidget *parent = nullptr);
  ~SessionWindow();

private:
  void setupUi(const QJsonObject &sessionData);
  void setupActions();
  void refreshSession();
  void onSessionReloaded(const QJsonObject &session);
  void duplicateSession();
  void updateAutoRefresh();

  QJsonObject m_sessionData;
  APIManager *m_apiManager;
  QTabWidget *m_tabWidget;
  QLabel *m_statusLabel;
  QTimer *m_autoRefreshTimer;
  QComboBox *m_autoRefreshCombo;
  QTextBrowser *m_detailsBrowser;
  QTextBrowser *m_activityBrowser;
  QTextBrowser *m_textBrowser;

  QWidget *m_activityTabWidget;
  class QLineEdit *m_chatInput;
};

#endif // SESSIONWINDOW_H
