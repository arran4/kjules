#ifndef SOURCEWINDOW_H
#define SOURCEWINDOW_H

#include <KXmlGuiWindow>

class SourceModel;
class SessionModel;
class QueueModel;
class ErrorsModel;
class BlockedTreeModel;
class APIManager;
class QTabWidget;
class QCheckBox;
class QSpinBox;
class QListWidget;
class QTextEdit;

class SourceWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit SourceWindow(const QString &sourceId, SourceModel *sourceModel, SessionModel *sessionModel,
                        QueueModel *queueModel, ErrorsModel *errorsModel, BlockedTreeModel *blockedTreeModel,
                        APIManager *apiManager, QWidget *parent = nullptr);
  ~SourceWindow() override;

private:
  void setupUi();
  void setupSettingsTab();
  void setupRawDataTab();
  void updateModels();
  void populateDefaultBranches();

  QString m_sourceId;
  SourceModel *m_sourceModel;
  SessionModel *m_sessionModel;
  QueueModel *m_queueModel;
  ErrorsModel *m_errorsModel;
  BlockedTreeModel *m_blockedTreeModel;
  APIManager *m_apiManager;

  QTabWidget *m_tabWidget;
  QCheckBox *m_autoFollowCheckBox;
  QSpinBox *m_concurrencySpinBox;
  QListWidget *m_defaultBranchesList;
  QTextEdit *m_rawDataEdit;
};

#endif // SOURCEWINDOW_H
