#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class APIManager;
class QLineEdit;
class QCheckBox;

class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(APIManager *apiManager, QWidget *parent = nullptr);

 private slots:
  void onTestConnection();
  void onSave();

 private:
  void loadSettings();
  void updateAutostart(bool enable);

  APIManager *m_apiManager;
  QLineEdit *m_apiKeyEdit;
  QLineEdit *m_githubTokenEdit;
  QCheckBox *m_autostartCheck;
};

#endif  // SETTINGSDIALOG_H
