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
                         bool isManaged = false, QWidget *parent = nullptr);
  ~SessionWindow();

private:
  void setupUi(const QJsonObject &sessionData);
  void setupActions(bool isManaged);
  void refreshSession();
  void onSessionReloaded(const QJsonObject &session);
  void onActivitiesReceived(const QString &sessionId,
                            const QJsonArray &activities);
  void duplicateSession();
  void updateAutoRefresh();
  void renderDetailsAndDiff();

  QJsonObject m_sessionData;
  APIManager *m_apiManager;
  QTabWidget *m_tabWidget;
  QLabel *m_statusLabel;
  QTimer *m_autoRefreshTimer;
  QComboBox *m_autoRefreshCombo;
  QTextBrowser *m_detailsBrowser;
  QTextBrowser *m_promptBrowser;
  QTextBrowser *m_diffBrowser;
  QTextBrowser *m_activityBrowser;
  QTextBrowser *m_rawActivitiesBrowser;
  QTextBrowser *m_textBrowser;

  QWidget *m_activityTabWidget;
  class QLineEdit *m_chatInput;

Q_SIGNALS:
  void archiveRequested(const QString &id);
  void deleteRequested(const QString &id);
  void templateRequested(const QJsonObject &templateData);
  void followRequested(const QJsonObject &sessionData);
};

#endif // SESSIONWINDOW_H
