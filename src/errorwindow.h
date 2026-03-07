#ifndef ERRORWINDOW_H
#define ERRORWINDOW_H

#include "queuemodel.h"
#include <QDialog>

class QTextEdit;
class QLabel;

class ErrorWindow : public QDialog {
  Q_OBJECT

public:
  explicit ErrorWindow(int queueRow, const QueueItem &item,
                       QWidget *parent = nullptr);

Q_SIGNALS:
  void editRequested(int row);
  void deleteRequested(int row);
  void draftRequested(int row);
  void sendNowRequested(int row);

private Q_SLOTS:
  void onCopyError();

private:
  int m_row;
  QueueItem m_item;

  QLabel *m_errorLabel;
  QTextEdit *m_rawRequestEdit;
  QTextEdit *m_rawResponseEdit;
};

#endif // ERRORWINDOW_H
