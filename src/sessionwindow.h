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

#include "errorsmodel.h"
class SessionErrorFilterProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
public:
  explicit SessionErrorFilterProxyModel(const QString &sessionId, QObject *parent = nullptr)
      : QSortFilterProxyModel(parent), m_sessionId(sessionId) {}
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    return sourceModel()->data(index, ErrorsModel::SessionIdRole).toString() ==
           m_sessionId; // SessionIdRole is usually +4 in ErrorsModel
  }

private:
  QString m_sessionId;
};

class SessionWindow : public KXmlGuiWindow {
  Q_OBJECT

Q_SIGNALS:
  void openPreviousAttemptRequested(const QString &previousAttemptId);

public:
  explicit SessionWindow(const QJsonObject &sessionData, APIManager *apiManager, ErrorsModel *errorsModel = nullptr,
                         bool isManaged = true, QWidget *parent = nullptr);
  ~SessionWindow();

private:
  void setupUi(const QJsonObject &sessionData);
  void setupActions();
  void refreshSession(bool isBackground = false);
  void onSessionReloaded(const QJsonObject &session);
  void onActivitiesReceived(const QString &sessionId, const QJsonArray &activities);
  void onMessageSent(const QString &sessionId);
  void onMessageSendFailed(const QString &sessionId, const QString &message, const QString &httpDetails);
  void duplicateSession();
  void updateAutoRefresh();
  void renderDetailsAndDiff();

  QJsonObject m_sessionData;
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
  void archiveRequested(const QString &id);
  void deleteRequested(const QString &id);
  void templateRequested(const QJsonObject &templateData);
  void refreshRequested(const QString &id);
};

#endif // SESSIONWINDOW_H
