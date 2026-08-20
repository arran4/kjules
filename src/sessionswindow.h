#ifndef SESSIONSWINDOW_H
#define SESSIONSWINDOW_H

#include <KXmlGuiWindow>

class APIManager;
class SessionModel;
class SessionsWidget;
class QActionGroup;
class QAction;

class SessionsWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit SessionsWindow(const QString &filterSource = QString(), APIManager *apiManager = nullptr,
                          SessionModel *managedModel = nullptr, QWidget *parent = nullptr);
  ~SessionsWindow() override;

Q_SIGNALS:
  void watchRequested(const QJsonObject &sessionData);
  void archiveRequested(const QString &id);
  void deleteRequested(const QString &id);

private:
  void setupActions();

  SessionsWidget *m_sessionsWidget;
  QAction *m_resumeAction;
  QAction *m_loadRemainingAction;
  QActionGroup *m_autoLoadGroup;
  QAction *m_autoFollowAction;
  QAction *m_watchMenuAction;
  QAction *m_archiveMenuAction;
  QAction *m_deleteMenuAction;
};

#endif // SESSIONSWINDOW_H
