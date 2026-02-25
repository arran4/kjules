#include "mainwindow.h"

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KStatusNotifierItem>
#include <QAction>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QStringListModel>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "apimanager.h"
#include "draftdelegate.h"
#include "draftsmodel.h"
#include "newsessiondialog.h"
#include "sessiondelegate.h"
#include "sessionmodel.h"
#include "settingsdialog.h"
#include "sourcemodel.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_apiManager(new APIManager(this)),
      m_sessionModel(new SessionModel(this)),
      m_sourceModel(new SourceModel(this)),
      m_draftsModel(new DraftsModel(this)),
      m_logModel(new QStringListModel(this)),
      m_refreshTimer(new QTimer(this)) {
  setupUi();
  setupTrayIcon();
  createActions();

  // Connect API Manager signals
  connect(m_apiManager, &APIManager::sourcesReceived, m_sourceModel,
          &SourceModel::setSources);
  connect(m_apiManager, &APIManager::sourcesAdded, m_sourceModel,
          &SourceModel::addSources);
  connect(m_apiManager, &APIManager::sessionsReceived, m_sessionModel,
          &SessionModel::setSessions);
  connect(m_apiManager, &APIManager::sessionDetailsReceived,
          [this](const QJsonObject &session) {
            m_sessionModel->updateSession(session);
            updateStatus(
                i18n("Session updated: %1", session.value("title").toString()));
          });
  connect(m_apiManager, &APIManager::sessionCreated,
          [this](const QJsonObject &session) {
            m_sessionModel->addSession(session);
            updateStatus(
                i18n("Session created: %1", session.value("title").toString()));
          });
  connect(m_apiManager, &APIManager::errorOccurred, this, &MainWindow::onError);
  connect(m_apiManager, &APIManager::logMessage, this,
          &MainWindow::updateStatus);

  // Initial refresh
  QTimer::singleShot(0, this, &MainWindow::refreshData);

  // Setup refresh timer (every 60 seconds)
  connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
  m_refreshTimer->start(60000);
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

  QTabWidget *tabWidget = new QTabWidget(this);

  // Sessions View
  m_sessionView = new QListView(this);
  m_sessionView->setModel(m_sessionModel);
  m_sessionView->setItemDelegate(new SessionDelegate(this));
  m_sessionView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_sessionView, &QListView::customContextMenuRequested,
      [this](const QPoint &pos) {
        QModelIndex index = m_sessionView->indexAt(pos);
        if (index.isValid()) {
          QMenu menu;
          QAction *openUrlAction = menu.addAction(i18n("Open URL"));
          QAction *copyUrlAction = menu.addAction(i18n("Copy URL"));

          connect(openUrlAction, &QAction::triggered, [this, index]() {
            QString id =
                m_sessionModel->data(index, SessionModel::IdRole).toString();
            // Placeholder URL logic
            updateStatus(i18n("Opening session %1", id));
          });

          connect(copyUrlAction, &QAction::triggered, [this, index]() {
            QString id =
                m_sessionModel->data(index, SessionModel::IdRole).toString();
            // Placeholder URL logic
            QGuiApplication::clipboard()->setText(
                "https://jules.google.com/sessions/" + id);
            updateStatus(i18n("URL copied to clipboard."));
          });
          menu.exec(m_sessionView->mapToGlobal(pos));
        }
      });
  connect(m_sessionView, &QListView::doubleClicked, this,
          &MainWindow::onSessionActivated);

  tabWidget->addTab(m_sessionView, i18n("Sessions"));

  // Drafts View
  m_draftsView = new QListView(this);
  m_draftsView->setModel(m_draftsModel);
  m_draftsView->setItemDelegate(new DraftDelegate(this));
  m_draftsView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_draftsView, &QListView::customContextMenuRequested,
          [this](const QPoint &pos) {
            QModelIndex index = m_draftsView->indexAt(pos);
            if (index.isValid()) {
              QMenu menu;
              QAction *submitAction = menu.addAction(i18n("Submit Now"));
              QAction *duplicateAction = menu.addAction(i18n("Duplicate"));
              QAction *deleteAction = menu.addAction(i18n("Delete"));

              connect(submitAction, &QAction::triggered,
                      [this, index]() { onDraftActivated(index); });

              connect(duplicateAction, &QAction::triggered, [this, index]() {
                QJsonObject draft = m_draftsModel->getDraft(index.row());
                m_draftsModel->addDraft(draft);
                updateStatus(i18n("Draft duplicated."));
              });

              connect(deleteAction, &QAction::triggered, [this, index]() {
                if (QMessageBox::question(this, i18n("Delete Draft"),
                                          i18n("Are you sure?")) ==
                    QMessageBox::Yes) {
                  m_draftsModel->removeDraft(index.row());
                }
              });

              menu.exec(m_draftsView->mapToGlobal(pos));
            }
          });
  connect(m_draftsView, &QListView::doubleClicked, this,
          &MainWindow::onDraftActivated);

  tabWidget->addTab(m_draftsView, i18n("Drafts"));

  // Log View
  m_logView = new QListView(this);
  m_logView->setModel(m_logModel);
  m_logView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  tabWidget->addTab(m_logView, i18n("Logs"));

  mainLayout->addWidget(tabWidget);

  // Toolbar / Buttons
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  QPushButton *refreshButton = new QPushButton(i18n("Refresh"), this);
  connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshData);

  QPushButton *newSessionButton = new QPushButton(i18n("New Session"), this);
  connect(newSessionButton, &QPushButton::clicked, this,
          &MainWindow::showNewSessionDialog);

  QPushButton *settingsButton = new QPushButton(i18n("Settings"), this);
  connect(settingsButton, &QPushButton::clicked, this,
          &MainWindow::showSettingsDialog);

  buttonLayout->addWidget(refreshButton);
  buttonLayout->addWidget(settingsButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(newSessionButton);

  mainLayout->addLayout(buttonLayout);

  // Status Bar
  m_statusLabel = new QLabel(i18n("Ready"), this);
  statusBar()->addWidget(m_statusLabel);
}

void MainWindow::setupTrayIcon() {
  m_trayIcon = new KStatusNotifierItem(this);
  m_trayIcon->setIconByName("sc-apps-kjules");
  m_trayIcon->setCategory(KStatusNotifierItem::ApplicationStatus);
  m_trayIcon->setStatus(KStatusNotifierItem::Active);
  m_trayIcon->setToolTip("sc-apps-kjules", i18n("kJules"),
                         i18n("Google Jules Client"));

  QMenu *menu = m_trayIcon->contextMenu();
  QAction *newSessionAction = menu->addAction(i18n("New Session"));
  connect(newSessionAction, &QAction::triggered, this,
          &MainWindow::showNewSessionDialog);

  connect(m_trayIcon, &KStatusNotifierItem::activateRequested, this,
          &MainWindow::toggleWindow);
}

void MainWindow::createActions() {
  QAction *newSessionAction = new QAction(i18n("New Session"), this);
  connect(newSessionAction, &QAction::triggered, this,
          &MainWindow::showNewSessionDialog);

  KActionCollection *collection = new KActionCollection(this);
  collection->addAction("new_session", newSessionAction);
  KGlobalAccel::setGlobalShortcut(newSessionAction,
                                  QKeySequence(Qt::CTRL + Qt::ALT + Qt::Key_N));
}

void MainWindow::refreshData() {
  updateStatus(i18n("Refreshing..."));
  m_apiManager->listSources();
  m_apiManager->listSessions();
}

void MainWindow::showNewSessionDialog() {
  NewSessionDialog *dialog = new NewSessionDialog(m_sourceModel, this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &NewSessionDialog::createSessionRequested, this,
          &MainWindow::onSessionCreated);
  connect(dialog, &NewSessionDialog::saveDraftRequested, this,
          &MainWindow::onDraftSaved);
  dialog->show();
}

void MainWindow::showSettingsDialog() {
  SettingsDialog dialog(m_apiManager, this);
  dialog.exec();
}

void MainWindow::onSessionCreated(const QStringList &sources,
                                  const QString &prompt,
                                  const QString &automationMode) {
  for (const QString &source : sources) {
    m_apiManager->createSession(source, prompt, automationMode);
  }
}

void MainWindow::onDraftSaved(const QJsonObject &draft) {
  m_draftsModel->addDraft(draft);
  updateStatus(i18n("Draft saved."));
}

void MainWindow::onDraftActivated(const QModelIndex &index) {
  QJsonObject draft = m_draftsModel->getDraft(index.row());
  NewSessionDialog *dialog = new NewSessionDialog(m_sourceModel, this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setInitialData(draft);

  // Use QPersistentModelIndex because row might change if other drafts are
  // removed? But drafts model doesn't change automatically unless user acts.
  // However, capturing index by value is safer if row doesn't change.
  // Actually, if we have multiple drafts open, submitting one removes it.
  // If we open draft at row 0, then open draft at row 1.
  // Submit draft 0. Row 0 removed. Draft 1 becomes row 0.
  // Submit draft 1 (using captured index row 1). Error!

  QPersistentModelIndex pIndex(index);

  connect(dialog, &NewSessionDialog::createSessionRequested,
          [this, pIndex](const QStringList &sources, const QString &p,
                         const QString &a) {
            onSessionCreated(sources, p, a);
            if (pIndex.isValid()) {
              m_draftsModel->removeDraft(pIndex.row());
            }
          });

  connect(dialog, &NewSessionDialog::saveDraftRequested,
          [this, pIndex](const QJsonObject &d) {
            if (pIndex.isValid()) {
              m_draftsModel->removeDraft(pIndex.row());
              m_draftsModel->addDraft(d);
              updateStatus(i18n("Draft updated."));
            }
          });

  dialog->show();
}

void MainWindow::onSessionActivated(const QModelIndex &index) {
  QString id = m_sessionModel->data(index, SessionModel::IdRole).toString();
  m_apiManager->getSession(id);
  updateStatus(i18n("Fetching details for session %1...", id));
}

void MainWindow::updateStatus(const QString &message) {
  m_statusLabel->setText(message);
  m_trayIcon->setToolTip("sc-apps-kjules", i18n("kJules"), message);

  // Add to log
  int row = m_logModel->rowCount();
  m_logModel->insertRow(row);
  m_logModel->setData(m_logModel->index(row, 0), message);
}

void MainWindow::onError(const QString &message) {
  QString errorMsg = i18n("Error: %1", message);
  updateStatus(errorMsg);
  QMessageBox::critical(this, i18n("Error"), message);
}

void MainWindow::toggleWindow() {
  if (isVisible()) {
    hide();
  } else {
    show();
    raise();
    activateWindow();
  }
}
