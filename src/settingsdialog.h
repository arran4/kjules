#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class APIManager;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QComboBox;
class QTabWidget;

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
  QComboBox *m_queueModeCombo;
  QSpinBox *m_oneAtATimeLimitEdit;
  QSpinBox *m_queueBackoffEdit;
  QSpinBox *m_refreshWorkersEdit;
  QSpinBox *m_pageSizeEdit;
  QComboBox *m_globalAutoRefreshCombo;
  QComboBox *m_followingAutoRefreshCombo;
  QCheckBox *m_autoArchiveCheckbox;
  QSpinBox *m_autoArchiveDaysEdit;
  QCheckBox *m_prMergeArchiveCheckbox;
  QLineEdit *m_snoozePresetsEdit;
};

#endif // SETTINGSDIALOG_H
