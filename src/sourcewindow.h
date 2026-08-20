#ifndef SOURCEWINDOW_H
#define SOURCEWINDOW_H

#include <KXmlGuiWindow>

class SourceModel;
class SessionModel;
class QueueModel;
class ErrorsModel;
class BlockedTreeModel;
class APIManager;
class SessionsWidget;
class QTabWidget;
class QCheckBox;
class QSpinBox;
class QListWidget;
class QTextEdit;

class SourceWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit SourceWindow(const QString &sourceId, SourceModel *sourceModel, SessionModel *sessionModel,
                        SessionModel *archiveModel, QueueModel *queueModel, ErrorsModel *errorsModel,
                        BlockedTreeModel *blockedTreeModel, APIManager *apiManager, QWidget *parent = nullptr);
  ~SourceWindow() override;

Q_SIGNALS:
  void newSessionRequested(const QString &sourceId);
  void queueProcessingRequested();

private:
  void setupUi();
  void setupFollowingTab();
  void setupArchivedTab();
  void setupQueuedBlockedTab();
  void setupSessionsTab();
  void setupSettingsTab();
  void setupRawDataTab();
  void populateDefaultBranches();

  QString m_sourceId;
  SourceModel *m_sourceModel;
  SessionModel *m_sessionModel;
  SessionModel *m_archiveModel;
  QueueModel *m_queueModel;
  ErrorsModel *m_errorsModel;
  BlockedTreeModel *m_blockedTreeModel;
  APIManager *m_apiManager;

  QTabWidget *m_tabWidget;
  SessionsWidget *m_sessionsWidget;
  QCheckBox *m_autoFollowCheckBox;
  QSpinBox *m_concurrencySpinBox;
  QListWidget *m_defaultBranchesList;
  QTextEdit *m_rawDataEdit;
};

#endif // SOURCEWINDOW_H
