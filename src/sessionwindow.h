#ifndef SESSIONWINDOW_H
#define SESSIONWINDOW_H

#include <KXmlGuiWindow>
#include <QJsonObject>

class QTextBrowser;
class QTextEdit;
class QTabWidget;
class QLabel;
class QTimer;
class QComboBox;
class APIManager;
class ActivityBrowser;

class SessionWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit SessionWindow(const QJsonObject &sessionData, APIManager *apiManager,
                         bool isManaged = true, QWidget *parent = nullptr);
  ~SessionWindow();

private:
  void setupUi(const QJsonObject &sessionData);
  void setupActions();
  void refreshSession();
  void onSessionReloaded(const QJsonObject &session);
  void onActivitiesReceived(const QString &sessionId,
                            const QJsonArray &activities);
  void duplicateSession();
  void updateAutoRefresh();
  void renderDetailsAndDiff();

public:
  void showNotesTab();

private:
  QJsonObject m_sessionData;
  APIManager *m_apiManager;
  bool m_isManaged;
  QTabWidget *m_tabWidget;
  QLabel *m_statusLabel;
  QTimer *m_autoRefreshTimer;
  QComboBox *m_autoRefreshCombo;
  QTextBrowser *m_detailsBrowser;
  QTextBrowser *m_promptBrowser;
  QTextBrowser *m_diffBrowser;
  QTextBrowser *m_prBrowser;
  ActivityBrowser *m_activityBrowser;
  QTextBrowser *m_rawActivitiesBrowser;
  QTextBrowser *m_textBrowser;

  QWidget *m_notesTabWidget;
  QTextEdit *m_notesEdit;

  QWidget *m_activityTabWidget;
  class QLineEdit *m_chatInput;

Q_SIGNALS:
  void watchRequested(const QJsonObject &sessionData);
  void archiveRequested(const QString &id);
  void deleteRequested(const QString &id);
  void templateRequested(const QJsonObject &templateData);
  void refreshRequested(const QString &id);
  void notesChanged(const QString &id, const QString &notes);
};

#endif // SESSIONWINDOW_H
