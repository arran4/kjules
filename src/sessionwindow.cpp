#include "sessionwindow.h"

#include <QJsonDocument>
#include <QTextBrowser>
#include <QVBoxLayout>

SessionWindow::SessionWindow(const QJsonObject &sessionData, QWidget *parent)
    : KXmlGuiWindow(parent) {
  setAttribute(Qt::WA_DeleteOnClose);
  setupUi(sessionData);
}

SessionWindow::~SessionWindow() {}

void SessionWindow::setupUi(const QJsonObject &sessionData) {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  m_textBrowser = new QTextBrowser(this);
  mainLayout->addWidget(m_textBrowser);

  QString title = sessionData.value(QStringLiteral("title")).toString();
  setWindowTitle(title.isEmpty() ? QStringLiteral("Session Details") : title);

  QJsonDocument doc(sessionData);
  QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

  m_textBrowser->setPlainText(jsonString);

  resize(800, 600);
}
