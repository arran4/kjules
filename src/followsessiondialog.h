#ifndef FOLLOWSESSIONDIALOG_H
#define FOLLOWSESSIONDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QMap>
#include <QStringList>

class QTextEdit;
class QLabel;
class QPushButton;
class APIManager;

class FollowSessionDialog : public QDialog {
  Q_OBJECT
public:
  explicit FollowSessionDialog(APIManager *apiManager, QWidget *parent = nullptr);
  QStringList sessionIds() const;
  QMap<QString, QJsonObject> sessionDataMap() const;

private Q_SLOTS:
  void onPreviewClicked();
  void onSessionReceived(const QJsonObject &session);
  void onSessionFailed(const QString &sessionId, const QString &message);
  void onErrorOccurred(const QString &error, bool isBackground);
  void updateButtons();

private:
  QStringList extractSessionIds(const QString &input) const;

  APIManager *m_apiManager;
  QTextEdit *m_inputEdit;
  QLabel *m_previewLabel;
  QPushButton *m_previewBtn;
  QPushButton *m_followBtn;
  QMap<QString, QJsonObject> m_sessionDataMap;
  int m_pendingPreviewCount = 0;
  QStringList m_previewIds;
};

#endif // FOLLOWSESSIONDIALOG_H
