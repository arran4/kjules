#ifndef REFRESHPROGRESSWINDOW_H
#define REFRESHPROGRESSWINDOW_H

#include <QDialog>
#include <QJsonObject>
#include <QMap>
#include <QMultiMap>
#include <QSet>
#include <QStringList>
#include <QUrl>

class QProgressBar;
class QTextBrowser;
class QPushButton;
class APIManager;
class SessionModel;

class RefreshProgressWindow : public QDialog {
  Q_OBJECT

public:
  explicit RefreshProgressWindow(const QStringList &sessionIds,
                                 APIManager *apiManager,
                                 SessionModel *sessionModel,
                                 QWidget *parent = nullptr);
  ~RefreshProgressWindow() override;

  void addSessionIds(const QStringList &ids);
  bool isFinishedProcess() const { return m_isFinished; }

Q_SIGNALS:
  void progressUpdated(int current, int total);
  void progressFinished();
  void openSessionRequested(const QString &id);

private Q_SLOTS:
  void processNext();
  void onAnchorClicked(const QUrl &url);
  void onSessionReloaded(const QJsonObject &session);

  void onSessionReloadFailed(const QString &sessionId, const QString &message);
  void onGithubPullRequestInfoReceived(const QString &prUrl,
                                       const QJsonObject &info);
  void onGithubPullRequestFailed(const QString &prUrl, const QString &message);

  void onSessionAutoArchived(const QString &id, const QString &reason);

private:
  QString getSessionLink(const QString &id) const;
  void finishCurrentTask(const QString &id);

  QProgressBar *m_progressBar;
  QTextBrowser *m_textBrowser;
  QPushButton *m_closeButton;

  QStringList m_queue;
  int m_totalCount;
  int m_currentIndex;
  int m_processedCount;
  int m_maxWorkers;
  QSet<QString> m_activeTasks;
  QMultiMap<QString, QString> m_activeTasksPrUrls; // prUrl -> id
  APIManager *m_apiManager;
  SessionModel *m_sessionModel;
  bool m_isFinished;
};

#endif // REFRESHPROGRESSWINDOW_H
