#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class APIManager;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QComboBox;

class SettingsDialog : public QDialog {
  Q_OBJECT

public:
  explicit SettingsDialog(APIManager *apiManager, QWidget *parent = nullptr);

private Q_SLOTS:
  void onTestConnection();
  void onSave();

private:
  APIManager *m_apiManager;
  QLineEdit *m_apiKeyEdit;
  QLineEdit *m_githubTokenEdit;
  QCheckBox *m_closeToTrayEdit;
  QSpinBox *m_queueIntervalEdit;
  QSpinBox *m_queueBackoffEdit;
  QSpinBox *m_waitTimeEdit;
  QComboBox *m_tierComboBox;
  QComboBox *m_globalAutoRefreshCombo;
  QCheckBox *m_autoArchiveEdit;
  QSpinBox *m_autoArchiveDaysEdit;
  QCheckBox *m_archiveOnMergedPREdit;
  QSpinBox *m_refreshFollowingIntervalEdit;
  QCheckBox *m_refreshOnOpenEdit;
  QCheckBox *m_notifyAwaitingFeedbackEdit;
  QCheckBox *m_notifyInProgressChangedEdit;
};

#endif // SETTINGSDIALOG_H
