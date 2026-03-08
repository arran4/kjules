#ifndef SOURCESESSIONSWINDOW_H
#define SOURCESESSIONSWINDOW_H

#include <KXmlGuiWindow>
#include <QSystemTrayIcon>

class APIManager;
class SessionModel;
class QListView;
class KStatusNotifierItem;
class QLabel;

class SourceSessionsWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit SourceSessionsWindow(const QString &sourceName,
                                QWidget *parent = nullptr);
  ~SourceSessionsWindow();

private Q_SLOTS:
  void refreshSessions();
  void onSessionActivated(const QModelIndex &index);
  void showSessionWindow(const QJsonObject &session);
  void updateStatus(const QString &message);
  void onError(const QString &message);
  void toggleWindow();

private:
  void setupUi();
  void setupTrayIcon();
  void createActions();

  APIManager *m_apiManager;
  SessionModel *m_sessionModel;
  QString m_sourceName;

  QListView *m_sessionView;
  KStatusNotifierItem *m_trayIcon;
  QLabel *m_statusLabel;
};

#endif // SOURCESESSIONSWINDOW_H
