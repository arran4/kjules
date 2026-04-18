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
  QCheckBox *m_autostartEdit;
  QCheckBox *m_autostartTrayEdit;
  QSpinBox *m_queueIntervalEdit;
  QSpinBox *m_queueBackoffEdit;
  QSpinBox *m_waitTimeEdit;
  QSpinBox *m_refreshWorkersEdit;
  QComboBox *m_tierComboBox;
  QComboBox *m_globalAutoRefreshCombo;
  QComboBox *m_followingAutoRefreshCombo;
  QCheckBox *m_autoArchiveCheckbox;
  QSpinBox *m_autoArchiveDaysEdit;
  QCheckBox *m_prMergeArchiveCheckbox;
};

#endif // SETTINGSDIALOG_H
