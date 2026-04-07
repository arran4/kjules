#ifndef ERRORWINDOW_H
#define ERRORWINDOW_H

#include "queuemodel.h"
#include <QDialog>
#include <QJsonObject>

class QTextEdit;
class QLabel;

class ErrorWindow : public QDialog {
  Q_OBJECT

public:
  explicit ErrorWindow(int queueRow, const QueueItem &item,
                       QWidget *parent = nullptr);
  explicit ErrorWindow(int errorRow, const QJsonObject &requestData,
                       const QString &lastResponse, const QString &lastError,
                       const QString &httpDetails = QString(),
                       QWidget *parent = nullptr);

Q_SIGNALS:
  void editRequested(int row);
  void deleteRequested(int row);
  void draftRequested(int row);
  void templateRequested(int row);
  void sendNowRequested(int row);
  void readdToQueueRequested(int row);

private Q_SLOTS:
  void onCopyError();

private:
  void setupUi();

  int m_row;
  QJsonObject m_requestData;
  QString m_lastResponse;
  QString m_lastError;
  QString m_httpDetails;

  QLabel *m_errorLabel;
  QTextEdit *m_rawRequestEdit;
  QTextEdit *m_rawResponseEdit;
  QTextEdit *m_httpDetailsEdit;
};

#endif // ERRORWINDOW_H
