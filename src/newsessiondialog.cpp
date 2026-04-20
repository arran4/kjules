#include "newsessiondialog.h"
#include "savedialog.h"
#include "templateselectiondialog.h"
#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>

class SourceSelectionProxyModel : public QSortFilterProxyModel {
public:
public:
  SourceSelectionProxyModel(const QMap<QString, QString> *selectedSources,
                            bool showSelected, QObject *parent = nullptr)
      : QSortFilterProxyModel(parent), m_selectedSources(selectedSources),
        m_showSelected(showSelected) {}

  void updateSelection() { invalidate(); }

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override {
    if (m_showSelected && role == Qt::DisplayRole) {
      QModelIndex sourceIdx = mapToSource(index);
      QString name =
          sourceModel()->data(sourceIdx, SourceModel::NameRole).toString();
      if (m_selectedSources->contains(name)) {
        QString branch = m_selectedSources->value(name);
        QString displayName =
            sourceModel()
                ->data(sourceIdx.siblingAtColumn(0), Qt::DisplayRole)
                .toString();
        return displayName + QStringLiteral(" (") + branch +
               QStringLiteral(")");
      }
    }
    return QSortFilterProxyModel::data(index, role);
  }

protected:
  bool lessThan(const QModelIndex &source_left,
                const QModelIndex &source_right) const override {
    bool leftFav =
        sourceModel()->data(source_left, SourceModel::FavouriteRole).toBool();
    bool rightFav =
        sourceModel()->data(source_right, SourceModel::FavouriteRole).toBool();

    if (leftFav != rightFav) {
      if (sortOrder() == Qt::AscendingOrder) {
        return leftFav;
      } else {
        return !leftFav;
      }
    }
    return QSortFilterProxyModel::lessThan(source_left, source_right);
  }

  bool filterAcceptsRow(int source_row,
                        const QModelIndex &source_parent) const override {
    if (!QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent))
      return false;

    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    QString name = sourceModel()->data(idx, SourceModel::NameRole).toString();
    bool isSelected = m_selectedSources->contains(name);
    return m_showSelected ? isSelected : !isSelected;
  }

private:
  const QMap<QString, QString> *m_selectedSources;
  bool m_showSelected;
};

NewSessionDialog::NewSessionDialog(SourceModel *sourceModel,
                                   TemplatesModel *templatesModel,
                                   bool hasApiKey, QWidget *parent)
    : KXmlGuiWindow(parent), m_sourceModel(sourceModel),
      m_templatesModel(templatesModel) {
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(tr("Create New Session"));

  KConfigGroup config(KSharedConfig::openConfig(),
                      QStringLiteral("NewSessionDialog"));
  if (!restoreGeometry(
          config.readEntry(QStringLiteral("Geometry"), QByteArray()))) {
    resize(700, 600);
  }

  QVBoxLayout *mainLayout = new QVBoxLayout();

  QFormLayout *formLayout = new QFormLayout();

  // Source Selection
  m_sourceSelectionWidget = new QWidget(this);
  QVBoxLayout *sourceLayout = new QVBoxLayout(m_sourceSelectionWidget);
  sourceLayout->setContentsMargins(0, 0, 0, 0);

  m_filterEdit = new QLineEdit(this);
  m_filterEdit->setPlaceholderText(tr("Filter sources..."));

  QPushButton *refreshSourcesBtn =
      new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")),
                      tr("Refresh Sources"), this);
  connect(refreshSourcesBtn, &QPushButton::clicked, this, [this]() {
    statusBar()->showMessage(tr("Requested refresh of sources..."));
    Q_EMIT refreshSourcesRequested();
  });

  QPushButton *refreshGithubBtn =
      new QPushButton(QIcon::fromTheme(QStringLiteral("network-server")),
                      tr("Refresh GitHub"), this);
  connect(refreshGithubBtn, &QPushButton::clicked, this, [this]() {
    statusBar()->showMessage(tr("Requested refresh of GitHub data..."));
    QStringList ids;
    for (const QString &name : m_selectedSources.keys()) {
      QModelIndexList matches = m_sourceModel->match(m_sourceModel->index(0, 0),
                                                     SourceModel::NameRole,
                                                     name, 1, Qt::MatchExactly);
      if (!matches.isEmpty()) {
        ids.append(matches.first().data(SourceModel::IdRole).toString());
      }
    }
    if (ids.isEmpty()) {
      for (int i = 0; i < m_unselectedProxy->rowCount() && i < 10; ++i) {
        QModelIndex srcIdx =
            m_unselectedProxy->mapToSource(m_unselectedProxy->index(i, 0));
        ids.append(srcIdx.data(SourceModel::IdRole).toString());
      }
    }
    Q_EMIT refreshGithubRequested(ids);
  });

  QHBoxLayout *filterLayout = new QHBoxLayout();
  filterLayout->addWidget(m_filterEdit);
  filterLayout->addWidget(refreshSourcesBtn);
  filterLayout->addWidget(refreshGithubBtn);
  sourceLayout->addLayout(filterLayout);

  QHBoxLayout *splitViewLayout = new QHBoxLayout();

  // Unselected List
  QVBoxLayout *unselectedLayout = new QVBoxLayout();
  unselectedLayout->addWidget(new QLabel(tr("Unselected Sources:"), this));

  m_unselectedView = new QListView(this);
  m_unselectedProxy =
      new SourceSelectionProxyModel(&m_selectedSources, false, this);
  m_unselectedProxy->setSourceModel(m_sourceModel);
  m_unselectedProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_unselectedProxy->setFilterRole(SourceModel::NameRole);
  m_unselectedProxy->sort(0, Qt::DescendingOrder);
  m_unselectedView->setModel(m_unselectedProxy);
  m_unselectedView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_unselectedView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_unselectedView, &QListView::customContextMenuRequested, this,
      [this](const QPoint &pos) {
        QPoint viewportPos =
            m_unselectedView->viewport()->mapFrom(m_unselectedView, pos);
        QModelIndex proxyIdx = m_unselectedView->indexAt(viewportPos);
        if (!proxyIdx.isValid())
          return;

        QModelIndex sourceIdx = m_unselectedProxy->mapToSource(proxyIdx);
        QString name =
            m_sourceModel->data(sourceIdx, SourceModel::NameRole).toString();

        QMenu menu(this);

        QAction *selectAction = menu.addAction(tr("Select"));
        connect(selectAction, &QAction::triggered, this, [this, proxyIdx]() {
          QModelIndexList selected =
              m_unselectedView->selectionModel()->selectedIndexes();
          if (!selected.contains(proxyIdx))
            selected = {proxyIdx};
          for (const QModelIndex &idx : selected) {
            QString name = idx.data(SourceModel::NameRole).toString();
            QModelIndex sourceIdx = m_unselectedProxy->mapToSource(idx);
            m_selectedSources.insert(name, getDefaultBranch(sourceIdx));
          }
          updateModels();
        });

        QAction *filterAction = menu.addAction(tr("Filter just this"));
        connect(filterAction, &QAction::triggered, this, [this, name]() {
          m_filterEdit->setText(name);
          applyFilter();
        });

        addFavouriteAction(menu, sourceIdx);

        menu.exec(m_unselectedView->mapToGlobal(pos));
      });

  unselectedLayout->addWidget(m_unselectedView);

  // Buttons in middle
  QVBoxLayout *middleButtonsLayout = new QVBoxLayout();
  middleButtonsLayout->addStretch();

  QPushButton *addBtn = new QPushButton(tr(">"), this);
  addBtn->setToolTip(tr("Add selected"));
  connect(addBtn, &QPushButton::clicked, this,
          &NewSessionDialog::onAddSelected);

  QPushButton *addAllBtn = new QPushButton(tr(">>"), this);
  addAllBtn->setToolTip(tr("Add all"));
  connect(addAllBtn, &QPushButton::clicked, this,
          &NewSessionDialog::onSelectAll);

  QPushButton *removeBtn = new QPushButton(tr("<"), this);
  removeBtn->setToolTip(tr("Remove selected"));
  connect(removeBtn, &QPushButton::clicked, this,
          &NewSessionDialog::onRemoveSelected);

  QPushButton *removeAllBtn = new QPushButton(tr("<<"), this);
  removeAllBtn->setToolTip(tr("Remove all"));
  connect(removeAllBtn, &QPushButton::clicked, this,
          &NewSessionDialog::onUnselectAll);

  middleButtonsLayout->addWidget(addBtn);
  middleButtonsLayout->addWidget(addAllBtn);
  middleButtonsLayout->addWidget(removeBtn);
  middleButtonsLayout->addWidget(removeAllBtn);
  middleButtonsLayout->addStretch();

  // Selected List
  QVBoxLayout *selectedLayout = new QVBoxLayout();
  selectedLayout->addWidget(new QLabel(tr("Selected Sources:"), this));

  m_selectedView = new QListView(this);
  m_selectedProxy =
      new SourceSelectionProxyModel(&m_selectedSources, true, this);
  m_selectedProxy->setSourceModel(m_sourceModel);
  m_selectedProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_selectedProxy->setFilterRole(SourceModel::NameRole);
  m_selectedProxy->sort(0, Qt::DescendingOrder);
  m_selectedView->setModel(m_selectedProxy);
  m_selectedView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_selectedView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_selectedView, &QListView::customContextMenuRequested, this,
      [this](const QPoint &pos) {
        QPoint viewportPos =
            m_selectedView->viewport()->mapFrom(m_selectedView, pos);
        QModelIndex proxyIdx = m_selectedView->indexAt(viewportPos);
        if (!proxyIdx.isValid())
          return;

        QModelIndex sourceIdx = m_selectedProxy->mapToSource(proxyIdx);
        QString name =
            m_sourceModel->data(sourceIdx, SourceModel::NameRole).toString();
        QString displayName =
            m_sourceModel->data(sourceIdx.siblingAtColumn(0), Qt::DisplayRole)
                .toString();

        QMenu menu(this);

        QAction *selectBranchAction = menu.addAction(tr("Select Branch..."));
        connect(selectBranchAction, &QAction::triggered, this,
                [this, name, displayName,
                 persistentSourceIdx = QPersistentModelIndex(sourceIdx)]() {
                  QModelIndex sourceIdx = persistentSourceIdx;
                  QString currentBranch = m_selectedSources.value(name);
                  bool ok;
                  QStringList branches = getAvailableBranches(sourceIdx);
                  int currentIndex = branches.indexOf(currentBranch);
                  if (currentIndex < 0) {
                    if (!currentBranch.isEmpty()) {
                      branches.prepend(currentBranch);
                    }
                    currentIndex = 0;
                  }
                  QString newBranch = QInputDialog::getItem(
                      this, tr("Select Branch"),
                      tr("Branch for %1:").arg(displayName), branches,
                      currentIndex, true, &ok);
                  if (ok && !newBranch.isEmpty()) {
                    m_selectedSources[name] = newBranch;
                    updateModels();
                  }
                });

        QAction *unselectAction = menu.addAction(tr("Unselect"));
        connect(unselectAction, &QAction::triggered, this, [this, proxyIdx]() {
          QModelIndexList selected =
              m_selectedView->selectionModel()->selectedIndexes();
          if (!selected.contains(proxyIdx))
            selected = {proxyIdx};
          for (const QModelIndex &idx : selected) {
            m_selectedSources.remove(
                idx.data(SourceModel::NameRole).toString());
          }
          updateModels();
        });

        addFavouriteAction(menu, sourceIdx);

        menu.exec(m_selectedView->mapToGlobal(pos));
      });

  selectedLayout->addWidget(m_selectedView);

  splitViewLayout->addLayout(unselectedLayout);
  splitViewLayout->addLayout(middleButtonsLayout);
  splitViewLayout->addLayout(selectedLayout);

  sourceLayout->addLayout(splitViewLayout);

  connect(m_filterEdit, &QLineEdit::textChanged, this,
          &NewSessionDialog::applyFilter);

  connect(m_filterEdit, &QLineEdit::returnPressed, this, [this]() {
    if (m_unselectedProxy->rowCount() == 1) {
      QModelIndex idx = m_unselectedProxy->index(0, 0);
      QString name = idx.data(SourceModel::NameRole).toString();
      QModelIndex sourceIdx = m_unselectedProxy->mapToSource(idx);
      m_selectedSources.insert(name, getDefaultBranch(sourceIdx));
      updateModels();
      m_filterEdit->clear();
    } else if (m_unselectedProxy->rowCount() > 1) {
      m_unselectedView->setFocus();
      m_unselectedView->setCurrentIndex(m_unselectedProxy->index(0, 0));
    }
  });

  connect(m_unselectedView, &QListView::activated, this,
          [this](const QModelIndex &idx) {
            QString name = idx.data(SourceModel::NameRole).toString();
            QModelIndex sourceIdx = m_unselectedProxy->mapToSource(idx);
            m_selectedSources.insert(name, getDefaultBranch(sourceIdx));
            updateModels();
            m_unselectedView->clearSelection();
          });

  connect(m_selectedView, &QListView::activated, this,
          [this](const QModelIndex &idx) {
            m_selectedSources.remove(
                idx.data(SourceModel::NameRole).toString());
            updateModels();
            m_selectedView->clearSelection();
          });

  formLayout->addRow(tr("Sources:"), m_sourceSelectionWidget);

  // Prompt
  m_promptEdit = new QTextEdit(this);
  m_loadTemplateButton = new QPushButton(tr("Load from template"), this);

  QVBoxLayout *promptLayout = new QVBoxLayout();
  promptLayout->addWidget(m_loadTemplateButton, 0, Qt::AlignLeft);
  promptLayout->addWidget(m_promptEdit);

  connect(m_promptEdit, &QTextEdit::textChanged, this, [this]() {
    m_loadTemplateButton->setVisible(
        m_promptEdit->toPlainText().trimmed().isEmpty());
  });

  formLayout->addRow(tr("Prompt:"), promptLayout);

  // Automation Mode
  m_automationModeComboBox = new QComboBox(this);
  m_automationModeComboBox->addItem(tr("Auto Create PR"),
                                    QStringLiteral("AUTO_CREATE_PR"));
  m_automationModeComboBox->addItem(
      tr("No Automation"), QStringLiteral("AUTOMATION_MODE_UNSPECIFIED"));
  formLayout->addRow(tr("Automation Mode:"), m_automationModeComboBox);

  // Options
  QHBoxLayout *optionsLayout = new QHBoxLayout();

  m_requirePlanApprovalCheckBox =
      new QCheckBox(tr("Require Plan Approval"), this);
  m_requirePlanApprovalCheckBox->setChecked(false);
  optionsLayout->addWidget(m_requirePlanApprovalCheckBox);

  m_keepOpenCheckBox = new QCheckBox(tr("Keep create new session open"), this);
  m_keepOpenCheckBox->setChecked(false);
  optionsLayout->addWidget(m_keepOpenCheckBox);

  m_keepSourceCheckBox = new QCheckBox(tr("Keep selected source"), this);
  m_keepSourceCheckBox->setChecked(false);
  optionsLayout->addWidget(m_keepSourceCheckBox);

  optionsLayout->addStretch();
  formLayout->addRow(tr("Options:"), optionsLayout);

  mainLayout->addLayout(formLayout);

  // Buttons
  QHBoxLayout *buttonLayout = new QHBoxLayout();

  QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);

  QPushButton *draftButton = new QPushButton(tr("Save as Draft"), this);

  m_saveTemplateButton = new QPushButton(tr("Save as Template"), this);

  m_createButton = new QPushButton(tr("Create Session"), this);
  m_createButton->setDefault(true);

  buttonLayout->addWidget(cancelButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(draftButton);
  buttonLayout->addWidget(m_saveTemplateButton);
  buttonLayout->addWidget(m_createButton);

  mainLayout->addLayout(buttonLayout);

  QWidget *centralWidget = new QWidget(this);
  centralWidget->setLayout(mainLayout);
  setCentralWidget(centralWidget);

  m_filterEdit->installEventFilter(this);
  m_unselectedView->installEventFilter(this);
  m_selectedView->installEventFilter(this);

  // Setup Actions
  QAction *closeAction =
      actionCollection()->addAction(QStringLiteral("file_close"));
  closeAction->setText(tr("&Close"));
  closeAction->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
  actionCollection()->setDefaultShortcut(closeAction, QKeySequence::Close);
  connect(closeAction, &QAction::triggered, this, &QWidget::close);
  connect(cancelButton, &QPushButton::clicked, closeAction, &QAction::trigger);

  QAction *loadTemplateAction =
      actionCollection()->addAction(QStringLiteral("load_template"));
  loadTemplateAction->setText(tr("&Load from template"));
  loadTemplateAction->setIcon(
      QIcon::fromTheme(QStringLiteral("document-open")));
  connect(loadTemplateAction, &QAction::triggered, this,
          &NewSessionDialog::onLoadTemplate);
  connect(m_loadTemplateButton, &QPushButton::clicked, loadTemplateAction,
          &QAction::trigger);

  QAction *saveTemplateAction =
      actionCollection()->addAction(QStringLiteral("save_template"));
  saveTemplateAction->setText(tr("Save as &Template"));
  saveTemplateAction->setIcon(
      QIcon::fromTheme(QStringLiteral("document-save-as")));
  connect(saveTemplateAction, &QAction::triggered, this,
          &NewSessionDialog::onSaveTemplate);
  connect(m_saveTemplateButton, &QPushButton::clicked, saveTemplateAction,
          &QAction::trigger);

  QAction *saveDraftAction =
      actionCollection()->addAction(QStringLiteral("save_draft"));
  saveDraftAction->setText(tr("Save as &Draft"));
  saveDraftAction->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
  connect(saveDraftAction, &QAction::triggered, this,
          &NewSessionDialog::onSaveDraft);
  connect(draftButton, &QPushButton::clicked, saveDraftAction,
          &QAction::trigger);

  QAction *createSessionAction =
      actionCollection()->addAction(QStringLiteral("create_session"));
  createSessionAction->setText(tr("Create &Session"));
  createSessionAction->setIcon(
      QIcon::fromTheme(QStringLiteral("media-playback-start")));
  actionCollection()->setDefaultShortcuts(
      createSessionAction, {QKeySequence(Qt::CTRL | Qt::Key_Enter),
                            QKeySequence(Qt::CTRL | Qt::Key_Return)});
  connect(createSessionAction, &QAction::triggered, this,
          &NewSessionDialog::onSubmitSession);
  connect(m_createButton, &QPushButton::clicked, createSessionAction,
          &QAction::trigger);

  if (!hasApiKey) {
    m_createButton->setEnabled(false);
    m_createButton->setToolTip(
        tr("An API key is required to create a session."));
    createSessionAction->setEnabled(false);
  }

  QAction *hideSelectedSourcesAction =
      actionCollection()->addAction(QStringLiteral("hide_selected_sources"));
  hideSelectedSourcesAction->setText(tr("&Hide Selected Sources"));
  hideSelectedSourcesAction->setCheckable(true);
  actionCollection()->setDefaultShortcut(hideSelectedSourcesAction,
                                         QKeySequence(Qt::CTRL | Qt::Key_H));
  connect(
      hideSelectedSourcesAction, &QAction::toggled, this,
      [this](bool checked) { m_sourceSelectionWidget->setVisible(!checked); });

  QAction *jumpToPromptAction =
      actionCollection()->addAction(QStringLiteral("jump_to_prompt"));
  jumpToPromptAction->setText(tr("Jump to &Prompt"));
  actionCollection()->setDefaultShortcut(jumpToPromptAction,
                                         QKeySequence(Qt::ALT | Qt::Key_P));
  connect(jumpToPromptAction, &QAction::triggered, m_promptEdit,
          qOverload<>(&QWidget::setFocus));

  // Sync checkboxes with actions
  QAction *requirePlanApprovalAction =
      actionCollection()->addAction(QStringLiteral("require_plan_approval"));
  requirePlanApprovalAction->setText(tr("Require &Plan Approval"));
  requirePlanApprovalAction->setCheckable(true);
  requirePlanApprovalAction->setChecked(
      m_requirePlanApprovalCheckBox->isChecked());
  actionCollection()->setDefaultShortcut(requirePlanApprovalAction,
                                         QKeySequence(Qt::CTRL | Qt::Key_P));
  connect(requirePlanApprovalAction, &QAction::toggled,
          m_requirePlanApprovalCheckBox, &QCheckBox::setChecked);
  connect(m_requirePlanApprovalCheckBox, &QCheckBox::toggled,
          requirePlanApprovalAction, &QAction::setChecked);

  QAction *keepOpenAction =
      actionCollection()->addAction(QStringLiteral("keep_open_after_saving"));
  keepOpenAction->setText(tr("&Keep window open after saving"));
  keepOpenAction->setCheckable(true);
  keepOpenAction->setChecked(m_keepOpenCheckBox->isChecked());
  actionCollection()->setDefaultShortcut(keepOpenAction,
                                         QKeySequence(Qt::CTRL | Qt::Key_K));
  connect(keepOpenAction, &QAction::toggled, m_keepOpenCheckBox,
          &QCheckBox::setChecked);
  connect(m_keepOpenCheckBox, &QCheckBox::toggled, keepOpenAction,
          &QAction::setChecked);

  QAction *keepSourceAction =
      actionCollection()->addAction(QStringLiteral("keep_source_selected"));
  keepSourceAction->setText(tr("Keep &source selected after saving"));
  keepSourceAction->setCheckable(true);
  keepSourceAction->setChecked(m_keepSourceCheckBox->isChecked());
  actionCollection()->setDefaultShortcut(keepSourceAction,
                                         QKeySequence(Qt::CTRL | Qt::Key_L));
  connect(keepSourceAction, &QAction::toggled, m_keepSourceCheckBox,
          &QCheckBox::setChecked);
  connect(m_keepSourceCheckBox, &QCheckBox::toggled, keepSourceAction,
          &QAction::setChecked);

  QAction *refreshSourcesAction =
      actionCollection()->addAction(QStringLiteral("refresh_sources"));
  refreshSourcesAction->setText(tr("Refresh Sources"));
  refreshSourcesAction->setIcon(
      QIcon::fromTheme(QStringLiteral("view-refresh")));
  connect(refreshSourcesAction, &QAction::triggered, this, [this]() {
    statusBar()->showMessage(tr("Refresh requested..."), 3000);
    Q_EMIT refreshSourcesRequested();
  });

  setupGUI(Default, QStringLiteral("newsessiondialogui.rc"));
}

void NewSessionDialog::onSubmitSession() {
  onSubmit(m_automationModeComboBox->currentData().toString());
}

void NewSessionDialog::onLoadTemplate() {
  TemplateSelectionDialog dlg(m_templatesModel, this);
  if (dlg.exec() == QDialog::Accepted) {
    setTemplateData(dlg.selectedTemplate());
  }
}

void NewSessionDialog::setEditMode(bool isEdit) {
  if (isEdit) {
    setWindowTitle(tr("Edit Queued Session"));
    m_createButton->setText(tr("Requeue Session"));
  } else {
    setWindowTitle(tr("Create New Session"));
    m_createButton->setText(tr("Create Session"));
  }
}

void NewSessionDialog::setInitialData(const QJsonObject &data) {
  QString prompt = data.value(QStringLiteral("prompt")).toString();

  if (data.contains(QStringLiteral("requirePlanApproval"))) {
    m_requirePlanApprovalCheckBox->setChecked(
        data.value(QStringLiteral("requirePlanApproval")).toBool());
  }

  if (data.contains(QStringLiteral("automationMode"))) {
    QString mode = data.value(QStringLiteral("automationMode")).toString();
    int idx = m_automationModeComboBox->findData(mode);
    if (idx != -1) {
      m_automationModeComboBox->setCurrentIndex(idx);
    }
  }

  if (data.contains(QStringLiteral("comment"))) {
    m_draftComment = data.value(QStringLiteral("comment")).toString();
  } else {
    m_draftComment.clear();
  }

  // Check for "sources" array, fallback to "source" string
  m_selectedSources.clear();
  if (data.contains(QStringLiteral("sources"))) {
    QJsonArray arr = data.value(QStringLiteral("sources")).toArray();
    for (const auto &val : arr) {
      if (val.isObject()) {
        QJsonObject sObj = val.toObject();
        QString name = sObj.value(QStringLiteral("name")).toString();
        QString branch = sObj.value(QStringLiteral("branch")).toString();
        m_selectedSources.insert(name, branch);
      } else {
        QString name = val.toString();
        QModelIndexList matches = m_sourceModel->match(
            m_sourceModel->index(0, 0), SourceModel::NameRole, name, 1,
            Qt::MatchExactly);
        if (!matches.isEmpty()) {
          m_selectedSources.insert(name, getDefaultBranch(matches.first()));
        } else {
          m_selectedSources.insert(name, QStringLiteral("main"));
        }
      }
    }
  } else if (data.contains(QStringLiteral("sourceContext")) &&
             data.value(QStringLiteral("sourceContext"))
                 .toObject()
                 .contains(QStringLiteral("sources"))) {
    QJsonArray arr = data.value(QStringLiteral("sourceContext"))
                         .toObject()
                         .value(QStringLiteral("sources"))
                         .toArray();
    for (const auto &val : arr) {
      QJsonObject sObj = val.toObject();
      QString name = sObj.value(QStringLiteral("name")).toString();
      QString branch = sObj.value(QStringLiteral("branch")).toString();
      m_selectedSources.insert(name, branch);
    }
  } else if (data.contains(QStringLiteral("source"))) {
    QString name = data.value(QStringLiteral("source")).toString();
    QString branch = data.value(QStringLiteral("startingBranch")).toString();
    if (branch.isEmpty()) {
      QModelIndexList matches = m_sourceModel->match(m_sourceModel->index(0, 0),
                                                     SourceModel::NameRole,
                                                     name, 1, Qt::MatchExactly);
      if (!matches.isEmpty()) {
        branch = getDefaultBranch(matches.first());
      } else {
        branch = QStringLiteral("main");
      }
    }
    m_selectedSources.insert(name, branch);
  } else if (data.contains(QStringLiteral("sourceContext"))) {
    QJsonObject sc = data.value(QStringLiteral("sourceContext")).toObject();
    if (sc.contains(QStringLiteral("source"))) {
      QString s = sc.value(QStringLiteral("source")).toString();
      if (s.startsWith(QStringLiteral("sources/"))) {
        s = s.mid(
            8); // Remove "sources/" prefix to match the NameRole in SourceModel
      }
      QString branch = QStringLiteral("main");
      if (sc.contains(QStringLiteral("githubRepoContext"))) {
        QJsonObject ghCtx =
            sc.value(QStringLiteral("githubRepoContext")).toObject();
        if (ghCtx.contains(QStringLiteral("startingBranch"))) {
          branch = ghCtx.value(QStringLiteral("startingBranch")).toString();
        }
      } else if (data.contains(QStringLiteral("startingBranch"))) {
        branch = data.value(QStringLiteral("startingBranch")).toString();
      }
      m_selectedSources.insert(s, branch);
    }
  }

  m_promptEdit->setPlainText(prompt);
  updateModels();
}

void NewSessionDialog::setTemplateData(const QJsonObject &data) {
  QString prompt = data.value(QStringLiteral("prompt")).toString();
  m_promptEdit->setPlainText(prompt);

  if (data.contains(QStringLiteral("requirePlanApproval"))) {
    m_requirePlanApprovalCheckBox->setChecked(
        data.value(QStringLiteral("requirePlanApproval")).toBool());
  }

  if (data.contains(QStringLiteral("automationMode"))) {
    QString mode = data.value(QStringLiteral("automationMode")).toString();
    int idx = m_automationModeComboBox->findData(mode);
    if (idx != -1) {
      m_automationModeComboBox->setCurrentIndex(idx);
    }
  }
}

void NewSessionDialog::updateModels() {
  m_unselectedProxy->updateSelection();
  m_selectedProxy->updateSelection();
  applyFilter();
}

void NewSessionDialog::applyFilter() {
  m_unselectedProxy->setFilterFixedString(m_filterEdit->text());
  if (m_selectedSources.size() < 10) {
    m_selectedProxy->setFilterFixedString(QStringLiteral(""));
  } else {
    m_selectedProxy->setFilterFixedString(m_filterEdit->text());
  }
}

void NewSessionDialog::onAddSelected() {
  QModelIndexList selection =
      m_unselectedView->selectionModel()->selectedIndexes();
  for (const QModelIndex &idx : selection) {
    QString name = idx.data(SourceModel::NameRole).toString();
    QModelIndex sourceIdx = m_unselectedProxy->mapToSource(idx);
    m_selectedSources.insert(name, getDefaultBranch(sourceIdx));
  }
  updateModels();
  m_unselectedView->clearSelection();
}

void NewSessionDialog::onRemoveSelected() {
  QModelIndexList selection =
      m_selectedView->selectionModel()->selectedIndexes();
  for (const QModelIndex &idx : selection) {
    m_selectedSources.remove(idx.data(SourceModel::NameRole).toString());
  }
  updateModels();
  m_selectedView->clearSelection();
}

void NewSessionDialog::onSelectAll() {
  for (int i = 0; i < m_unselectedProxy->rowCount(); ++i) {
    QModelIndex idx = m_unselectedProxy->index(i, 0);
    QString name = idx.data(SourceModel::NameRole).toString();
    QModelIndex sourceIdx = m_unselectedProxy->mapToSource(idx);
    m_selectedSources.insert(name, getDefaultBranch(sourceIdx));
  }
  updateModels();
}

void NewSessionDialog::onUnselectAll() {
  for (int i = 0; i < m_selectedProxy->rowCount(); ++i) {
    QModelIndex idx = m_selectedProxy->index(i, 0);
    m_selectedSources.remove(idx.data(SourceModel::NameRole).toString());
  }
  updateModels();
}

void NewSessionDialog::onSubmit(const QString &automationMode) {
  if (m_selectedSources.isEmpty()) {
    QMessageBox::warning(this, tr("Missing Source"),
                         tr("Please select at least one source."));
    return;
  }

  QMap<QString, QString> sources = m_selectedSources;

  QString prompt = m_promptEdit->toPlainText();

  if (prompt.isEmpty()) {
    QMessageBox::warning(this, tr("Missing Prompt"),
                         tr("Please enter a prompt."));
    return;
  }

  bool requirePlanApproval = m_requirePlanApprovalCheckBox->isChecked();

  Q_EMIT createSessionRequested(sources, prompt, automationMode,
                                requirePlanApproval);

  if (m_keepOpenCheckBox->isChecked()) {
    m_promptEdit->clear();
    if (!m_keepSourceCheckBox->isChecked()) {
      m_selectedSources.clear();
      updateModels();
    }
  } else {
    close();
  }
}

void NewSessionDialog::onSaveDraft() {
  SaveDialog dlg(QStringLiteral("Draft"), this);
  if (!m_draftComment.isEmpty()) {
    dlg.setInitialData(m_draftComment);
  }
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  QJsonArray sourcesArr;
  for (auto it = m_selectedSources.constBegin();
       it != m_selectedSources.constEnd(); ++it) {
    QJsonObject sObj;
    sObj[QStringLiteral("name")] = it.key();
    sObj[QStringLiteral("branch")] = it.value();
    sourcesArr.append(sObj);
  }

  QString prompt = m_promptEdit->toPlainText();
  bool requirePlanApproval = m_requirePlanApprovalCheckBox->isChecked();

  QJsonObject draft;
  draft[QStringLiteral("sources")] = sourcesArr;
  draft[QStringLiteral("prompt")] = prompt;
  draft[QStringLiteral("comment")] = dlg.nameOrComment();
  draft[QStringLiteral("requirePlanApproval")] = requirePlanApproval;
  draft[QStringLiteral("automationMode")] =
      m_automationModeComboBox->currentData().toString();

  Q_EMIT saveDraftRequested(draft);
  close();
}

void NewSessionDialog::onSaveTemplate() {
  SaveDialog dlg(QStringLiteral("Template"), this);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  QJsonArray sourcesArr;
  for (auto it = m_selectedSources.constBegin();
       it != m_selectedSources.constEnd(); ++it) {
    QJsonObject sObj;
    sObj[QStringLiteral("name")] = it.key();
    sObj[QStringLiteral("branch")] = it.value();
    sourcesArr.append(sObj);
  }

  QString prompt = m_promptEdit->toPlainText();
  bool requirePlanApproval = m_requirePlanApprovalCheckBox->isChecked();

  QJsonObject tmpl;
  // A template might not necessarily need sources but we save them anyway
  // as the requirement "a template doesn't set the sources in an existing new
  // session window" means we ignore sources when loading in an *existing*
  // window.
  tmpl[QStringLiteral("sources")] = sourcesArr;
  tmpl[QStringLiteral("prompt")] = prompt;
  tmpl[QStringLiteral("name")] = dlg.nameOrComment();
  tmpl[QStringLiteral("description")] = dlg.description();
  tmpl[QStringLiteral("requirePlanApproval")] = requirePlanApproval;
  tmpl[QStringLiteral("automationMode")] =
      m_automationModeComboBox->currentData().toString();

  Q_EMIT saveTemplateRequested(tmpl);
  // We do not close the dialog when saving a template, it can be used multiple
  // times
}

void NewSessionDialog::showEvent(QShowEvent *event) {
  KXmlGuiWindow::showEvent(event);
  if (!m_selectedSources.isEmpty()) {
    m_promptEdit->setFocus();
  } else {
    m_filterEdit->setFocus();
  }
}

void NewSessionDialog::hideEvent(QHideEvent *event) {
  KXmlGuiWindow::hideEvent(event);
  KConfigGroup config(KSharedConfig::openConfig(),
                      QStringLiteral("NewSessionDialog"));
  config.writeEntry(QStringLiteral("Geometry"), saveGeometry());
}

bool NewSessionDialog::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    auto focusList = [](QListView *view, QSortFilterProxyModel *proxy) {
      view->setFocus();
      if (!view->currentIndex().isValid() && proxy->rowCount() > 0) {
        view->setCurrentIndex(proxy->index(0, 0));
      }
      return true;
    };

    auto handleUp = [this](QListView *view) {
      QModelIndex currentIdx = view->currentIndex();
      if (!currentIdx.isValid() || currentIdx.row() == 0) {
        m_filterEdit->setFocus();
        return true;
      }
      return false;
    };

    if (obj == m_filterEdit && keyEvent->key() == Qt::Key_Down) {
      if (m_unselectedProxy->rowCount() > 0) {
        return focusList(m_unselectedView, m_unselectedProxy);
      }
    } else if (obj == m_unselectedView) {
      if (keyEvent->key() == Qt::Key_Right) {
        return focusList(m_selectedView, m_selectedProxy);
      } else if (keyEvent->key() == Qt::Key_Up) {
        if (handleUp(m_unselectedView))
          return true;
      }
    } else if (obj == m_selectedView) {
      if (keyEvent->key() == Qt::Key_Left) {
        return focusList(m_unselectedView, m_unselectedProxy);
      } else if (keyEvent->key() == Qt::Key_Up) {
        if (handleUp(m_selectedView))
          return true;
      }
    }
  }
  return KXmlGuiWindow::eventFilter(obj, event);
}

QString NewSessionDialog::getDefaultBranch(const QModelIndex &sourceIdx) {
  QJsonObject rawData =
      m_sourceModel->data(sourceIdx, SourceModel::RawDataRole).toJsonObject();
  if (rawData.contains(QStringLiteral("defaultBranch"))) {
    return rawData.value(QStringLiteral("defaultBranch")).toString();
  }
  QJsonObject github = rawData.value(QStringLiteral("github")).toObject();
  if (github.contains(QStringLiteral("default_branch"))) {
    return github.value(QStringLiteral("default_branch")).toString();
  }
  return QStringLiteral("main");
}

QStringList
NewSessionDialog::getAvailableBranches(const QModelIndex &sourceIdx) {
  QStringList branches;
  QSet<QString> seen;
  QJsonObject rawData =
      m_sourceModel->data(sourceIdx, SourceModel::RawDataRole).toJsonObject();

  auto addUnique = [&](const QJsonArray &arr) {
    for (const QJsonValue &v : arr) {
      QString b = v.toString();
      if (!b.isEmpty() && !seen.contains(b)) {
        branches.append(b);
        seen.insert(b);
      }
    }
  };

  // Extract from github info if available
  QJsonObject github = rawData.value(QStringLiteral("github")).toObject();
  addUnique(github.value(QStringLiteral("branches")).toArray());

  // Merge with possible API branches
  addUnique(rawData.value(QStringLiteral("branches")).toArray());

  // Determine default branch
  QString defaultBranch = getDefaultBranch(sourceIdx);

  // If no branches known, fallback to defaultBranch and some standard ones
  if (branches.isEmpty()) {
    if (!defaultBranch.isEmpty()) {
      addUnique(QJsonArray{defaultBranch});
    }
    addUnique(QJsonArray{QStringLiteral("main"), QStringLiteral("master")});
  }

  // Ensure default branch is at the top
  QString topBranch;
  if (!defaultBranch.isEmpty() && seen.contains(defaultBranch)) {
    topBranch = defaultBranch;
  } else if (seen.contains(QStringLiteral("main"))) {
    topBranch = QStringLiteral("main");
  } else if (seen.contains(QStringLiteral("master"))) {
    topBranch = QStringLiteral("master");
  }

  if (!topBranch.isEmpty()) {
    branches.removeAll(topBranch);
    branches.prepend(topBranch);
  }

  return branches;
}

void NewSessionDialog::updateStatus(const QString &message) {
  statusBar()->showMessage(message);
}

void NewSessionDialog::addFavouriteAction(QMenu &menu,
                                          const QModelIndex &sourceIdx) {
  QString id = m_sourceModel->data(sourceIdx, SourceModel::IdRole).toString();
  int isFavourite =
      m_sourceModel->data(sourceIdx, SourceModel::FavouriteRole).toInt();

  if (isFavourite > 0) {
    QAction *favouriteAction = menu.addAction(tr("Unfavourite"));
    connect(favouriteAction, &QAction::triggered, this,
            [this, id]() { m_sourceModel->toggleFavourite(id); });

    QAction *moveUpAction = menu.addAction(
        QIcon::fromTheme(QStringLiteral("go-up")), tr("Move Favourite Up"));
    connect(moveUpAction, &QAction::triggered, this,
            [this, id]() { m_sourceModel->moveFavouriteUp(id); });

    QAction *moveDownAction = menu.addAction(
        QIcon::fromTheme(QStringLiteral("go-down")), tr("Move Favourite Down"));
    connect(moveDownAction, &QAction::triggered, this,
            [this, id]() { m_sourceModel->moveFavouriteDown(id); });
  } else {
    QAction *favouriteAction = menu.addAction(tr("Favourite"));
    connect(favouriteAction, &QAction::triggered, this,
            [this, id]() { m_sourceModel->toggleFavourite(id); });
  }
}
