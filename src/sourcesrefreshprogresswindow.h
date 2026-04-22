#ifndef SOURCESREFRESHPROGRESSWINDOW_H
#define SOURCESREFRESHPROGRESSWINDOW_H

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class QProgressBar;
class QTextBrowser;
class QPushButton;
class APIManager;

class SourcesRefreshProgressWindow : public QDialog {
  Q_OBJECT

public:
  explicit SourcesRefreshProgressWindow(APIManager *apiManager,
                                        QWidget *parent = nullptr);

  void appendLog(const QString &msg);
  void setProgress(int current, int total);
  void reset();

private Q_SLOTS:
  void onSourcesReceived(const QJsonArray &sources);
  void onGithubInfoReceived(const QString &sourceId, const QJsonObject &info);
  void onGithubInfoFailed(const QString &sourceId, const QString &message);
  void onSourcesRefreshFinished();
  void processNextGithub();

private:
  QProgressBar *m_progressBar;
  QTextBrowser *m_textBrowser;
  QPushButton *m_closeButton;

  APIManager *m_apiManager;
  int m_totalGithubRequests;
  int m_finishedGithubRequests;
  int m_activeWorkers;
  QStringList m_githubQueue;
};

#endif // SOURCESREFRESHPROGRESSWINDOW_H
