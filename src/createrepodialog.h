#ifndef CREATEREPODIALOG_H
#define CREATEREPODIALOG_H

#include "newsessiondialog.h"
#include <KXmlGuiWindow>
#include <QJsonObject>

class QLineEdit;
class QCheckBox;
class QComboBox;
class QPushButton;
class APIManager;
class PromptTextEdit;

class CreateRepoDialog : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit CreateRepoDialog(APIManager *apiManager, QWidget *parent = nullptr);

Q_SIGNALS:
  void createRepoAndSessionRequested(const QString &org,
                                     const QString &repoName, bool isPrivate,
                                     const QString &prompt,
                                     const QString &automationMode,
                                     bool requirePlanApproval);

public Q_SLOTS:
  void updateStatus(const QString &message);

private Q_SLOTS:
  void onSubmit();
  void onGithubUsernameFetched(const QString &username);

protected:
  void showEvent(QShowEvent *event) override;

private:
  APIManager *m_apiManager;

  QLineEdit *m_orgEdit;
  QLineEdit *m_repoNameEdit;
  QCheckBox *m_privateCheckBox;

  PromptTextEdit *m_promptEdit;
  QComboBox *m_automationModeComboBox;
  QCheckBox *m_requirePlanApprovalCheckBox;

  QPushButton *m_createButton;
};

#endif // CREATEREPODIALOG_H
