#ifndef FOLLOWSESSIONDIALOG_H
#define FOLLOWSESSIONDIALOG_H

#include <QDialog>
#include <QJsonObject>

class QLineEdit;
class QLabel;
class QPushButton;
class APIManager;

class FollowSessionDialog : public QDialog {
  Q_OBJECT
public:
  explicit FollowSessionDialog(APIManager *apiManager,
                               QWidget *parent = nullptr);
  QString sessionId() const;
  QJsonObject sessionData() const;

private Q_SLOTS:
  void onPreviewClicked();
  void onSessionReceived(const QJsonObject &session);
  void onErrorOccurred(const QString &error);
  void updateButtons();

private:
  QString extractSessionId(const QString &input) const;

  APIManager *m_apiManager;
  QLineEdit *m_inputEdit;
  QLabel *m_previewLabel;
  QPushButton *m_previewBtn;
  QPushButton *m_followBtn;
  QJsonObject m_sessionData;
};

#endif // FOLLOWSESSIONDIALOG_H
