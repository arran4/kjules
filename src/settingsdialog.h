#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class APIManager;
class QLineEdit;
class QLabel;
class QComboBox;

class SettingsDialog : public QDialog {
  Q_OBJECT

public:
  explicit SettingsDialog(APIManager *apiManager, QWidget *parent = nullptr);

Q_SIGNALS:
  void tierChanged(int tierIndex);

private Q_SLOTS:
  void onTestApiKey();
  void onTestGithubToken();
  void onSave();

private:
  APIManager *m_apiManager;
  QLineEdit *m_apiKeyEdit;
  QLineEdit *m_githubTokenEdit;
  QLabel *m_apiKeyStatus;
  QLabel *m_githubTokenStatus;
  QComboBox *m_tierComboBox;
};

#endif // SETTINGSDIALOG_H
