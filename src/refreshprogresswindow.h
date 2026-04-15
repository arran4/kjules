#ifndef REFRESHPROGRESSWINDOW_H
#define REFRESHPROGRESSWINDOW_H

#include <QDialog>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QStringList>

class QProgressBar;
class QTextBrowser;
class QPushButton;
class APIManager;

class RefreshProgressWindow : public QDialog {
  Q_OBJECT

public:
  explicit RefreshProgressWindow(const QStringList &sessionIds,
                                 APIManager *apiManager,
                                 QWidget *parent = nullptr);
  ~RefreshProgressWindow() override;

private Q_SLOTS:
  void processNext();
  void onSessionReloaded(const QJsonObject &session);
  void onSessionReloadFailed(const QString &sessionId, const QString &message);
  void onGithubPullRequestInfoReceived(const QString &prUrl,
                                       const QJsonObject &info);
  void onGithubPullRequestFailed(const QString &prUrl, const QString &message);

private:
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
};

#endif // REFRESHPROGRESSWINDOW_H
