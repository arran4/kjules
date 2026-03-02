#ifndef SESSIONWINDOW_H
#define SESSIONWINDOW_H

#include <KXmlGuiWindow>
#include <QJsonObject>

class QTextBrowser;

class SessionWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit SessionWindow(const QJsonObject &sessionData,
                         QWidget *parent = nullptr);
  ~SessionWindow();

private:
  void setupUi(const QJsonObject &sessionData);

  QTextBrowser *m_textBrowser;
};

#endif // SESSIONWINDOW_H
