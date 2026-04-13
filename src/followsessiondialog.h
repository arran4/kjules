#ifndef FOLLOWSESSIONDIALOG_H
#define FOLLOWSESSIONDIALOG_H

#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;
class APIManager;
class SessionModel;

class FollowSessionDialog : public QDialog {
  Q_OBJECT

public:
  explicit FollowSessionDialog(APIManager *apiManager, SessionModel *sessionModel, QWidget *parent = nullptr);

private Q_SLOTS:
  void previewSession();
  void followSession();

private:
  QLineEdit *m_idEdit;
  QLabel *m_previewLabel;
  QPushButton *m_previewBtn;
  QPushButton *m_followBtn;
  APIManager *m_apiManager;
  SessionModel *m_sessionModel;

  QString extractSessionId(const QString &input) const;
};

#endif // FOLLOWSESSIONDIALOG_H
