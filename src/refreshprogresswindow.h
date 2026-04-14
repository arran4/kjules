#ifndef REFRESHPROGRESSWINDOW_H
#define REFRESHPROGRESSWINDOW_H

#include <QDialog>
#include <QJsonObject>
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
  void onErrorOccurred(const QString &message);
  void onSessionAutoArchived(const QString &id, const QString &reason);

private:
  QProgressBar *m_progressBar;
  QTextBrowser *m_textBrowser;
  QPushButton *m_closeButton;

  QStringList m_queue;
  int m_totalCount;
  int m_currentIndex;
  APIManager *m_apiManager;
};

#endif // REFRESHPROGRESSWINDOW_H
