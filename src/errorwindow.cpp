#include "errorwindow.h"

#include <KLocalizedString>
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

ErrorWindow::ErrorWindow(int queueRow, const QueueItem &item, QWidget *parent)
    : QDialog(parent), m_row(queueRow), m_requestData(item.requestData),
      m_lastResponse(item.lastResponse), m_lastError(item.lastError) {
  setWindowTitle(i18n("Queue Error Details"));
  setupUi();
}

ErrorWindow::ErrorWindow(int errorRow, const QJsonObject &requestData,
                         const QString &lastResponse, const QString &lastError,
                         const QString &httpDetails, QWidget *parent)
    : QDialog(parent), m_row(errorRow), m_requestData(requestData),
      m_lastResponse(lastResponse), m_lastError(lastError),
      m_httpDetails(httpDetails) {
  setWindowTitle(i18n("Error Details"));
  setupUi();
}

void ErrorWindow::setupUi() {
  resize(600, 450);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  QTabWidget *tabWidget = new QTabWidget(this);

  // Tab 1: Error Details
  QWidget *detailsTab = new QWidget(tabWidget);
  QVBoxLayout *detailsLayout = new QVBoxLayout(detailsTab);

  // Centered Error Message
  detailsLayout->addStretch();
  m_errorLabel = new QLabel(m_lastError, this);
  m_errorLabel->setAlignment(Qt::AlignCenter);
  m_errorLabel->setWordWrap(true);
  QFont font = m_errorLabel->font();
  font.setPointSize(font.pointSize() + 2);
  font.setBold(true);
  m_errorLabel->setFont(font);
  m_errorLabel->setStyleSheet(QStringLiteral("color: red;"));
  detailsLayout->addWidget(m_errorLabel);
  detailsLayout->addStretch();

  // Actions layout (centered)
  QHBoxLayout *actionsLayout = new QHBoxLayout();
  actionsLayout->addStretch();

  QPushButton *editBtn = new QPushButton(
      QIcon::fromTheme(QStringLiteral("document-edit")), i18n("Edit"), this);
  connect(editBtn, &QPushButton::clicked, [this]() {
    Q_EMIT editRequested(m_row);
    accept();
  });

  QPushButton *deleteBtn = new QPushButton(
      QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete"), this);
  connect(deleteBtn, &QPushButton::clicked, [this]() {
    if (QMessageBox::question(this, i18n("Remove Task"),
                              i18n("Remove this task from the queue?")) ==
        QMessageBox::Yes) {
      Q_EMIT deleteRequested(m_row);
      accept();
    }
  });

  QPushButton *draftBtn =
      new QPushButton(QIcon::fromTheme(QStringLiteral("document-save-as")),
                      i18n("Convert to Draft"), this);
  connect(draftBtn, &QPushButton::clicked, [this]() {
    Q_EMIT draftRequested(m_row);
    accept();
  });

  QPushButton *templateBtn =
      new QPushButton(QIcon::fromTheme(QStringLiteral("edit-copy")),
                      i18n("Copy as Template"), this);
  connect(templateBtn, &QPushButton::clicked, [this]() {
    Q_EMIT templateRequested(m_row);
    accept();
  });

  QPushButton *sendNowBtn = new QPushButton(
      QIcon::fromTheme(QStringLiteral("mail-send")), i18n("Send Now"), this);
  connect(sendNowBtn, &QPushButton::clicked, [this]() {
    Q_EMIT sendNowRequested(m_row);
    accept();
  });

  QPushButton *copyErrorBtn = new QPushButton(
      QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy Error"), this);
  connect(copyErrorBtn, &QPushButton::clicked, this, &ErrorWindow::onCopyError);

  actionsLayout->addWidget(editBtn);
  actionsLayout->addWidget(deleteBtn);
  actionsLayout->addWidget(draftBtn);
  actionsLayout->addWidget(templateBtn);
  actionsLayout->addWidget(sendNowBtn);
  actionsLayout->addWidget(copyErrorBtn);
  actionsLayout->addStretch();

  detailsLayout->addLayout(actionsLayout);
  tabWidget->addTab(detailsTab, i18n("Error Details"));

  // Tab 2: Raw Data
  QWidget *rawTab = new QWidget(tabWidget);
  QVBoxLayout *rawLayout = new QVBoxLayout(rawTab);

  QLabel *reqLabel = new QLabel(i18n("Raw Request:"), this);
  m_rawRequestEdit = new QTextEdit(this);
  m_rawRequestEdit->setReadOnly(true);
  m_rawRequestEdit->setPlainText(QString::fromUtf8(
      QJsonDocument(m_requestData).toJson(QJsonDocument::Indented)));

  QLabel *resLabel = new QLabel(i18n("Raw Response:"), this);
  m_rawResponseEdit = new QTextEdit(this);
  m_rawResponseEdit->setReadOnly(true);
  m_rawResponseEdit->setPlainText(m_lastResponse);

  rawLayout->addWidget(reqLabel);
  rawLayout->addWidget(m_rawRequestEdit);
  rawLayout->addWidget(resLabel);
  rawLayout->addWidget(m_rawResponseEdit);
  tabWidget->addTab(rawTab, i18n("Raw Data"));

  // Tab 3: HTTP Details
  if (!m_httpDetails.isEmpty()) {
    QWidget *httpTab = new QWidget(tabWidget);
    QVBoxLayout *httpLayout = new QVBoxLayout(httpTab);

    m_httpDetailsEdit = new QTextEdit(this);
    m_httpDetailsEdit->setReadOnly(true);
    m_httpDetailsEdit->setPlainText(m_httpDetails);

    httpLayout->addWidget(m_httpDetailsEdit);
    tabWidget->addTab(httpTab, i18n("HTTP Details"));
  }

  mainLayout->addWidget(tabWidget);

  // Close button
  QHBoxLayout *bottomLayout = new QHBoxLayout();
  bottomLayout->addStretch();
  QPushButton *closeBtn = new QPushButton(i18n("Close"), this);
  connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
  bottomLayout->addWidget(closeBtn);
  mainLayout->addLayout(bottomLayout);
}

void ErrorWindow::onCopyError() {
  QClipboard *clipboard = QGuiApplication::clipboard();
  clipboard->setText(m_lastError);
}
