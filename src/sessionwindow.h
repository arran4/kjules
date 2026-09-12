#ifndef SESSIONWINDOW_H
#define SESSIONWINDOW_H

#include <KXmlGuiWindow>
#include <QJsonObject>
#include <QSortFilterProxyModel>

class QTextBrowser;
class QTabWidget;
class QLabel;
class QTimer;
class QComboBox;
class APIManager;
class ActivityBrowser;
class ErrorsModel;
class ClickableLabel;
class QSplitter;
class QStackedWidget;
class QListWidget;
class QListWidgetItem;
class JobStore;
#include "errorsmodel.h"
class SessionErrorFilterProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit SessionErrorFilterProxyModel(const QString &sessionId, QObject *parent = nullptr)
      : QSortFilterProxyModel(parent), m_sessionId(sessionId) {}

  void setFilterTarget(const QString &jobId, const QString &attemptId, const QString &sessionId) {
    m_jobId = jobId;
    m_attemptId = attemptId;
    m_sessionId = sessionId;
    invalidate();
  }

  void setJobAndAttempt(const QString &jobId, const QString &attemptId) {
    m_jobId = jobId;
    m_attemptId = attemptId;
    invalidate();
  }

  void setSessionId(const QString &id) {
    m_sessionId = id;
    invalidate();
  }

  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
    if (!sourceModel())
      return false;
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    QString errorJobId = sourceModel()->data(index, ErrorsModel::JobIdRole).toString();
    QString errorAttemptId = sourceModel()->data(index, ErrorsModel::AttemptIdRole).toString();
    QString errorSessionId = sourceModel()->data(index, ErrorsModel::SessionIdRole).toString();

    // If this proxy is configured for a Job-aware window
    if (!m_jobId.isEmpty()) {
      // If error has a jobId: it must match this job and this attempt
      if (!errorJobId.isEmpty()) {
        return errorJobId == m_jobId && errorAttemptId == m_attemptId;
      }
      // If error does not have a jobId, but has a sessionId:
      // It can match if the current attempt has a valid remote sessionId and they match
      if (!m_sessionId.isEmpty() && !errorSessionId.isEmpty()) {
        return errorSessionId == m_sessionId;
      }
      return false;
    }

    // Standalone legacy SessionWindow: match on sessionId
    if (!m_sessionId.isEmpty() && !errorSessionId.isEmpty()) {
      return errorSessionId == m_sessionId;
    }
    return false;
  }

private:
  QString m_jobId;
  QString m_attemptId;
  QString m_sessionId;
};

class SessionWindow : public KXmlGuiWindow {
  Q_OBJECT

Q_SIGNALS:
  void openPreviousAttemptRequested(const QString &previousAttemptId);

public:
  explicit SessionWindow(const QString &jobId, JobStore *jobStore, APIManager *apiManager,
                         ErrorsModel *errorsModel = nullptr, bool isManaged = true, QWidget *parent = nullptr);
  explicit SessionWindow(const QJsonObject &sessionData, APIManager *apiManager, ErrorsModel *errorsModel = nullptr,
                         bool isManaged = true, QWidget *parent = nullptr);

  ~SessionWindow();

public:
  QString jobId() const { return m_jobId; }
  QJsonObject currentVariantRequest() const;
  QJsonObject currentSessionData() const;

private:
  void setupUi(const QJsonObject &sessionData);
  void renderZeroAttempts();
  void updateAttemptList();
  void onAttemptSelected(QListWidgetItem *item);
  void setupActions();
  void updateActionStates();

  void refreshSession(bool isBackground = false);
  void onSessionReloaded(const QJsonObject &session, bool isBackground);
  void onActivitiesReceived(const QString &sessionId, const QJsonArray &activities);
  void onMessageSent(const QString &sessionId);
  void onMessageSendFailed(const QString &sessionId, const QString &message, const QString &httpDetails);
  void duplicateSession();
  void updateAutoRefresh();
  void renderDetailsAndDiff();

  QJsonObject m_sessionData;
  QString m_jobId;
  JobStore *m_jobStore = nullptr;
  QString m_currentAttemptId;

  QSplitter *m_splitter;
  QListWidget *m_attemptList;
  QStackedWidget *m_contentStack;
  QWidget *m_zeroAttemptWidget;
  QWidget *m_detailsWidget;

  APIManager *m_apiManager;

  bool m_isManaged;
  QString m_statusErrorDetails;
  QTabWidget *m_tabWidget;
  ErrorsModel *m_errorsModel;
  ClickableLabel *m_statusLabel;
  ClickableLabel *m_unseenErrorLabel;
  QTimer *m_autoRefreshTimer;
  QComboBox *m_autoRefreshCombo;
  QTextBrowser *m_detailsBrowser;
  QTextBrowser *m_promptBrowser;
  QTextBrowser *m_diffBrowser;
  QTextBrowser *m_prBrowser;
  ActivityBrowser *m_activityBrowser;
  QTextBrowser *m_rawActivitiesBrowser;
  QTextBrowser *m_textBrowser;

  QWidget *m_activityTabWidget;
  QWidget *m_errorTab;
  class QLineEdit *m_chatInput;
  class QPushButton *m_sendButton;
  QString m_pendingMessage;

Q_SIGNALS:
  void watchRequested(const QJsonObject &sessionData);
  void duplicateRequested(const QJsonObject &sessionData);
  void newAttemptRequested(const QString &jobId, const QJsonObject &request);
  void retryAttemptRequested(const QString &jobId, const QString &attemptId);
  void variantRequested(const QString &jobId, const QJsonObject &request);
  void newJobFromRequested(const QJsonObject &request);
  void archiveRequested(const QString &id);
  void jobMutated(const QString &jobId);
  void deleteRequested(const QString &id);
  void templateRequested(const QJsonObject &templateData);
  void refreshRequested(const QString &id);
};

#endif // SESSIONWINDOW_H
