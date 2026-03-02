#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KXmlGuiWindow>
#include <QSystemTrayIcon>

class APIManager;
class SessionModel;
class SourceModel;
class DraftsModel;
class QListView;
class KStatusNotifierItem;
class QLabel;

class MainWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private Q_SLOTS:
  void refreshSources();
  void refreshSessions();
  void showNewSessionDialog();
  void showSettingsDialog();
  void onSessionCreated(const QStringList &sources, const QString &prompt,
                        const QString &automationMode);
  void onDraftSaved(const QJsonObject &draft);
  void onDraftActivated(const QModelIndex &index);
  void onSessionActivated(const QModelIndex &index);
  void onSourceActivated(const QModelIndex &index);
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
  SourceModel *m_sourceModel;
  DraftsModel *m_draftsModel;

  QListView *m_sourceView;
  QListView *m_sessionView;
  QListView *m_draftsView;
  KStatusNotifierItem *m_trayIcon;
  QLabel *m_statusLabel;
};

#endif // MAINWINDOW_H
