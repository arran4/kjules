#include "newsessiondialog.h"
#include "filtereditor.h"
#include "filterparser.h"
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
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>

PromptTextEdit::PromptTextEdit(QWidget *parent)
    : QTextEdit(parent), m_mode(WysiwygMarkdown) {
  setAcceptRichText(true);
}

void PromptTextEdit::setMarkdownMode(int modeInt) {
  Mode mode = static_cast<Mode>(modeInt);
  if (m_mode == mode)
    return;

  if (mode == RawMarkdown) {
    QString md = toMarkdown();
    clear();
    setPlainText(md);
    setAcceptRichText(false);
  } else if (mode == WysiwygMarkdown) {
    QString raw = toPlainText();
    clear();
    setMarkdown(raw);
    setAcceptRichText(true);
  }
  m_mode = mode;
}

PromptTextEdit::Mode PromptTextEdit::currentMode() const { return m_mode; }

QString PromptTextEdit::getPromptText() const {
  if (m_mode == WysiwygMarkdown) {
    return toMarkdown();
  } else {
    return toPlainText();
  }
}

void PromptTextEdit::setPromptText(const QString &text) {
  if (m_mode == WysiwygMarkdown) {
    setMarkdown(text);
  } else {
    setPlainText(text);
  }
}

void PromptTextEdit::insertFromMimeData(const QMimeData *source) {
  if (m_mode == RawMarkdown) {
    if (source->hasHtml()) {
      QTextEdit temp;
      temp.setHtml(source->html());
      insertPlainText(temp.toMarkdown());
      return;
    } else if (source->hasText()) {
      insertPlainText(source->text());
      return;
    }
  }
  QTextEdit::insertFromMimeData(source);
}

class SourceSelectionProxyModel : public QSortFilterProxyModel {
public:
  SourceSelectionProxyModel(const QMultiMap<QString, QString> *selectedSources,
                            bool showSelected, QObject *parent = nullptr)
      : QSortFilterProxyModel(parent), m_selectedSources(selectedSources),
        m_showSelected(showSelected), m_hideArchived(true), m_hideForks(false),
        m_hidePrivate(false), m_hidePublic(false) {
    setFilterKeyColumn(-1);
  }

  void setHideArchived(bool hide) {
    m_hideArchived = hide;
    invalidate();
  }

  void setHideForks(bool hide) {
    m_hideForks = hide;
    invalidate();
  }

  void setHidePrivate(bool hide) {
    m_hidePrivate = hide;
    invalidate();
  }

  void setHidePublic(bool hide) {
    m_hidePublic = hide;
    invalidate();
  }

  void updateSelection() { invalidate(); }

  void setFilterAST(QSharedPointer<ASTNode> ast) {
    m_ast = ast;
    invalidate();
  }

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override {
    if (m_showSelected && role == Qt::DisplayRole) {
      QModelIndex sourceIdx = mapToSource(index);
      QString name =
          sourceModel()->data(sourceIdx, SourceModel::NameRole).toString();
      if (m_selectedSources->contains(name)) {
        QStringList branches = m_selectedSources->values(name);
        branches.sort();
        QString displayName =
            sourceModel()
                ->data(sourceIdx.siblingAtColumn(0), Qt::DisplayRole)
                .toString();
        return displayName + QStringLiteral(" (") +
               branches.join(QStringLiteral(", ")) + QStringLiteral(")");
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

  class ProxyFilterDataAccessor : public FilterDataAccessor {
  public:
    ProxyFilterDataAccessor(const QModelIndex &index, const SourceModel *model)
        : m_index(index), m_model(model) {}

    QString getValue(const QString &key) const override {
      QString k = key.toLower();
      if (k == QStringLiteral("repo") || k == QStringLiteral("name")) {
        return m_model->data(m_index, SourceModel::NameRole).toString();
      } else if (k == QStringLiteral("owner")) {
        QString id = m_model->data(m_index, SourceModel::IdRole).toString();
        QStringList parts = id.split(QLatin1Char('/'));
        if (parts.size() >= 3 && parts[0] == QStringLiteral("sources")) {
          return parts[2];
        }
      } else if (k == QStringLiteral("title")) {
        return m_model->data(m_index.siblingAtColumn(0), Qt::DisplayRole)
            .toString();
      }

      QJsonObject rawData =
          m_model->data(m_index, SourceModel::RawDataRole).toJsonObject();
      QJsonObject github = rawData.value(QStringLiteral("github")).toObject();

      if (k == QStringLiteral("archived") ||
          k == QStringLiteral("isarchived")) {
        if (rawData.contains(QStringLiteral("isArchived"))) {
          return rawData.value(QStringLiteral("isArchived")).toBool()
                     ? QStringLiteral("true")
                     : QStringLiteral("false");
        }
        return github.value(QStringLiteral("archived")).toBool()
                   ? QStringLiteral("true")
                   : QStringLiteral("false");
      } else if (k == QStringLiteral("fork") || k == QStringLiteral("isfork")) {
        if (rawData.contains(QStringLiteral("isFork"))) {
          return rawData.value(QStringLiteral("isFork")).toBool()
                     ? QStringLiteral("true")
                     : QStringLiteral("false");
        }
        return github.value(QStringLiteral("fork")).toBool()
                   ? QStringLiteral("true")
                   : QStringLiteral("false");
      } else if (k == QStringLiteral("private") ||
                 k == QStringLiteral("isprivate")) {
        if (rawData.contains(QStringLiteral("isPrivate"))) {
          return rawData.value(QStringLiteral("isPrivate")).toBool()
                     ? QStringLiteral("true")
                     : QStringLiteral("false");
        }
        return github.value(QStringLiteral("private")).toBool()
                   ? QStringLiteral("true")
                   : QStringLiteral("false");
      }

      return QString();
    }

    QList<QString> getAllValues() const override {
      QList<QString> values;
      values.append(m_model->data(m_index, SourceModel::NameRole).toString());
      values.append(m_model->data(m_index.siblingAtColumn(0), Qt::DisplayRole)
                        .toString());
      return values;
    }

  private:
    const QModelIndex &m_index;
    const SourceModel *m_model;
  };

  bool filterAcceptsRow(int source_row,
                        const QModelIndex &source_parent) const override {
    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);

    // First apply AST filtering if present
    if (m_ast) {
      ProxyFilterDataAccessor accessor(
          idx, qobject_cast<SourceModel *>(sourceModel()));
      if (!m_ast->evaluate(accessor)) {
        return false;
      }
    } else {
      if (!QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent))
        return false;
    }

    QString name = sourceModel()->data(idx, SourceModel::NameRole).toString();
    bool isSelected = m_selectedSources->contains(name);

    if (m_showSelected) {
      return isSelected;
    }

    if (isSelected) {
      return false;
    }

    if (m_hideArchived || m_hideForks || m_hidePrivate || m_hidePublic) {
      QJsonObject rawData =
          sourceModel()->data(idx, SourceModel::RawDataRole).toJsonObject();

      if (m_hideArchived) {
        if (rawData.value(QStringLiteral("isArchived")).toBool()) {
          return false;
        }
        const QJsonObject github =
            rawData.value(QStringLiteral("github")).toObject();
        if (github.value(QStringLiteral("archived")).toBool()) {
          return false;
        }
      }

      if (m_hideForks) {
        if (rawData.value(QStringLiteral("isFork")).toBool()) {
          return false;
        }
        const QJsonObject github =
            rawData.value(QStringLiteral("github")).toObject();
        if (github.value(QStringLiteral("fork")).toBool()) {
          return false;
        }
      }

      if (m_hidePrivate || m_hidePublic) {
        bool isPrivate = false;
        bool hasPrivacyInfo = false;

        if (rawData.contains(QStringLiteral("isPrivate"))) {
          isPrivate = rawData.value(QStringLiteral("isPrivate")).toBool();
          hasPrivacyInfo = true;
        } else if (rawData.contains(QStringLiteral("github"))) {
          const QJsonObject github =
              rawData.value(QStringLiteral("github")).toObject();
          if (github.contains(QStringLiteral("private"))) {
            isPrivate = github.value(QStringLiteral("private")).toBool();
            hasPrivacyInfo = true;
          }
        }

        if (hasPrivacyInfo) {
          if ((m_hidePrivate && isPrivate) || (m_hidePublic && !isPrivate)) {
            return false;
          }
        }
      }
    }

    return true;
  }

private:
  const QMultiMap<QString, QString> *m_selectedSources;
  bool m_showSelected;
  bool m_hideArchived;
  bool m_hideForks;
  bool m_hidePrivate;
  bool m_hidePublic;
  QSharedPointer<ASTNode> m_ast;
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
  bool hideArchived = config.readEntry(QStringLiteral("HideArchived"), true);
  bool hideForks = config.readEntry(QStringLiteral("HideForks"), false);
  bool hidePrivate = config.readEntry(QStringLiteral("HidePrivate"), false);
  bool hidePublic = config.readEntry(QStringLiteral("HidePublic"), false);

  m_unselectedProxy =
      new SourceSelectionProxyModel(&m_selectedSources, false, this);
  m_unselectedProxy->setSourceModel(m_sourceModel);
  m_unselectedProxy->setHideArchived(hideArchived);
  m_unselectedProxy->setHideForks(hideForks);
  m_unselectedProxy->setHidePrivate(hidePrivate);
  m_unselectedProxy->setHidePublic(hidePublic);
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

        QString id =
            m_sourceModel->data(sourceIdx, SourceModel::IdRole).toString();
        QStringList parts = id.split(QLatin1Char('/'));
        QString owner;
        QString repo;
        if (parts.size() >= 4 && parts[0] == QStringLiteral("sources")) {
          owner = parts[2];
          repo = parts[3];
        }

        if (!owner.isEmpty() && !repo.isEmpty()) {
          QAction *hideRepoAction = menu.addAction(i18n("Hide this repo"));
          QAction *hideOwnerAction = menu.addAction(i18n("Hide this owner"));
          QAction *onlyRepoAction = menu.addAction(i18n("Only this repo"));
          QAction *onlyOwnerAction = menu.addAction(i18n("Only this owner"));

          connect(hideRepoAction, &QAction::triggered, [this, repo]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("repo"), repo, true));
            applyFilter();
          });
          connect(hideOwnerAction, &QAction::triggered, [this, owner]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("owner"), owner, true));
            applyFilter();
          });
          connect(onlyRepoAction, &QAction::triggered, [this, repo]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("repo"), repo, false));
            applyFilter();
          });
          connect(onlyOwnerAction, &QAction::triggered, [this, owner]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("owner"), owner, false));
            applyFilter();
          });
        }

        QJsonObject rawData =
            m_sourceModel->data(sourceIdx, SourceModel::RawDataRole)
                .toJsonObject();
        if (rawData.contains(QStringLiteral("github"))) {
          QJsonObject github =
              rawData.value(QStringLiteral("github")).toObject();

          bool isArchived = github.value(QStringLiteral("archived")).toBool();
          bool isFork = github.value(QStringLiteral("fork")).toBool();
          bool isPrivate = github.value(QStringLiteral("private")).toBool();

          QAction *archivedAction =
              menu.addAction(isArchived ? i18n("Filter out archived")
                                        : i18n("Filter only archived"));
          connect(archivedAction, &QAction::triggered, [this, isArchived]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("archived"),
                QStringLiteral("true"), isArchived));
            applyFilter();
          });

          QAction *forkAction = menu.addAction(
              isFork ? i18n("Filter out forks") : i18n("Filter only forks"));
          connect(forkAction, &QAction::triggered, [this, isFork]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("fork"),
                QStringLiteral("true"), isFork));
            applyFilter();
          });

          QAction *privateAction =
              menu.addAction(isPrivate ? i18n("Filter out private")
                                       : i18n("Filter only private"));
          connect(privateAction, &QAction::triggered, [this, isPrivate]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("private"),
                QStringLiteral("true"), isPrivate));
            applyFilter();
          });
        }

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
        connect(
            selectBranchAction, &QAction::triggered, this,
            [this, proxyIdx, name, displayName,
             persistentSourceIdx = QPersistentModelIndex(sourceIdx)]() {
              QModelIndex sourceIdx = persistentSourceIdx;
              QModelIndexList selected =
                  m_selectedView->selectionModel()->selectedIndexes();
              if (!selected.contains(proxyIdx))
                selected = {proxyIdx};

              QStringList currentBranches = m_selectedSources.values(name);
              QStringList branches = getAvailableBranches(sourceIdx);

              QDialog dialog(this);
              dialog.setWindowTitle(tr("Select Branch"));
              QVBoxLayout layout(&dialog);

              if (selected.size() > 1) {
                layout.addWidget(new QLabel(tr(
                    "Branch for %n selected source(s):", "", selected.size())));
              } else {
                layout.addWidget(
                    new QLabel(tr("Branch for %1:").arg(displayName)));
              }

              // Single mode
              QWidget *singleModeWidget = new QWidget();
              QHBoxLayout *singleLayout = new QHBoxLayout(singleModeWidget);
              singleLayout->setContentsMargins(0, 0, 0, 0);

              QComboBox *comboBox = new QComboBox(&dialog);
              comboBox->addItems(branches);
              if (!currentBranches.isEmpty()) {
                int idx = branches.indexOf(currentBranches.first());
                if (idx >= 0) {
                  comboBox->setCurrentIndex(idx);
                } else {
                  branches.prepend(currentBranches.first());
                  comboBox->clear();
                  comboBox->addItems(branches);
                  comboBox->setCurrentIndex(0);
                }
              }
              comboBox->setEditable(true);

              QPushButton *plusBtn =
                  new QPushButton(QStringLiteral("+"), &dialog);
              plusBtn->setToolTip(tr("Switch to multiple branch selection"));
              plusBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

              singleLayout->addWidget(comboBox);
              singleLayout->addWidget(plusBtn);

              // Multi mode
              QWidget *multiModeWidget = new QWidget();
              QVBoxLayout *multiLayout = new QVBoxLayout(multiModeWidget);
              multiLayout->setContentsMargins(0, 0, 0, 0);

              QListWidget *listWidget = new QListWidget(&dialog);
              listWidget->addItems(currentBranches);

              QHBoxLayout *addRemoveLayout = new QHBoxLayout();
              QComboBox *multiComboBox = new QComboBox(&dialog);
              multiComboBox->addItems(branches);
              multiComboBox->setEditable(true);

              QPushButton *addBtn = new QPushButton(tr("Add"), &dialog);
              QPushButton *removeBtn = new QPushButton(tr("Remove"), &dialog);

              addRemoveLayout->addWidget(multiComboBox);
              addRemoveLayout->addWidget(addBtn);
              addRemoveLayout->addWidget(removeBtn);

              multiLayout->addWidget(listWidget);
              multiLayout->addLayout(addRemoveLayout);

              layout.addWidget(singleModeWidget);
              layout.addWidget(multiModeWidget);

              bool isMultiMode = currentBranches.size() > 1;
              if (isMultiMode) {
                singleModeWidget->hide();
              } else {
                multiModeWidget->hide();
              }

              connect(plusBtn, &QPushButton::clicked, [&]() {
                isMultiMode = true;
                singleModeWidget->hide();
                multiModeWidget->show();
                dialog.adjustSize();
                if (listWidget->count() == 0 &&
                    !comboBox->currentText().isEmpty()) {
                  listWidget->addItem(comboBox->currentText());
                }
              });

              connect(addBtn, &QPushButton::clicked, [&]() {
                QString t = multiComboBox->currentText();
                if (!t.isEmpty() &&
                    listWidget->findItems(t, Qt::MatchExactly).isEmpty()) {
                  listWidget->addItem(t);
                }
              });

              connect(removeBtn, &QPushButton::clicked,
                      [&]() { qDeleteAll(listWidget->selectedItems()); });

              QHBoxLayout *btnLayout = new QHBoxLayout();
              QPushButton *refreshJulesBtn =
                  new QPushButton(tr("Refresh Jules"), &dialog);
              QPushButton *refreshGithubBtn =
                  new QPushButton(tr("Refresh GitHub"), &dialog);
              btnLayout->addWidget(refreshJulesBtn);
              btnLayout->addWidget(refreshGithubBtn);
              btnLayout->addStretch();
              layout.addLayout(btnLayout);

              QDialogButtonBox *buttonBox = new QDialogButtonBox(
                  QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
              layout.addWidget(buttonBox);

              connect(buttonBox, &QDialogButtonBox::accepted, &dialog,
                      &QDialog::accept);
              connect(buttonBox, &QDialogButtonBox::rejected, &dialog,
                      &QDialog::reject);

              QStringList selectedIds;
              for (const QModelIndex &idx : selected) {
                selectedIds.append(idx.data(SourceModel::IdRole).toString());
              }

              connect(refreshJulesBtn, &QPushButton::clicked, this,
                      [this, selectedIds]() {
                        for (const QString &id : selectedIds) {
                          Q_EMIT refreshSourceRequested(id);
                        }
                        updateStatus(
                            tr("Requested Jules refresh for %n source(s)...",
                               "", selectedIds.size()));
                      });

              connect(refreshGithubBtn, &QPushButton::clicked, this,
                      [this, selectedIds]() {
                        Q_EMIT refreshGithubRequested(selectedIds);
                        updateStatus(
                            tr("Requested GitHub refresh for %n source(s)...",
                               "", selectedIds.size()));
                      });

              if (dialog.exec() == QDialog::Accepted) {
                QStringList newBranches;
                if (isMultiMode) {
                  for (int i = 0; i < listWidget->count(); ++i) {
                    newBranches.append(listWidget->item(i)->text());
                  }
                } else {
                  QString t = comboBox->currentText();
                  if (!t.isEmpty())
                    newBranches.append(t);
                }

                if (!newBranches.isEmpty()) {
                  for (const QModelIndex &idx : selected) {
                    QString selName =
                        idx.data(SourceModel::NameRole).toString();
                    m_selectedSources.remove(selName);
                    for (const QString &b : newBranches) {
                      m_selectedSources.insert(selName, b);
                    }
                  }
                  updateModels();
                }
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

        QString id =
            m_sourceModel->data(sourceIdx, SourceModel::IdRole).toString();
        QStringList parts = id.split(QLatin1Char('/'));
        QString owner;
        QString repo;
        if (parts.size() >= 4 && parts[0] == QStringLiteral("sources")) {
          owner = parts[2];
          repo = parts[3];
        }

        if (!owner.isEmpty() && !repo.isEmpty()) {
          QAction *hideRepoAction = menu.addAction(i18n("Hide this repo"));
          QAction *hideOwnerAction = menu.addAction(i18n("Hide this owner"));
          QAction *onlyRepoAction = menu.addAction(i18n("Only this repo"));
          QAction *onlyOwnerAction = menu.addAction(i18n("Only this owner"));

          connect(hideRepoAction, &QAction::triggered, [this, repo]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("repo"), repo, true));
            applyFilter();
          });
          connect(hideOwnerAction, &QAction::triggered, [this, owner]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("owner"), owner, true));
            applyFilter();
          });
          connect(onlyRepoAction, &QAction::triggered, [this, repo]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("repo"), repo, false));
            applyFilter();
          });
          connect(onlyOwnerAction, &QAction::triggered, [this, owner]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("owner"), owner, false));
            applyFilter();
          });
        }

        QJsonObject rawData =
            m_sourceModel->data(sourceIdx, SourceModel::RawDataRole)
                .toJsonObject();
        if (rawData.contains(QStringLiteral("github"))) {
          QJsonObject github =
              rawData.value(QStringLiteral("github")).toObject();

          bool isArchived = github.value(QStringLiteral("archived")).toBool();
          bool isFork = github.value(QStringLiteral("fork")).toBool();
          bool isPrivate = github.value(QStringLiteral("private")).toBool();

          QAction *archivedAction =
              menu.addAction(isArchived ? i18n("Filter out archived")
                                        : i18n("Filter only archived"));
          connect(archivedAction, &QAction::triggered, [this, isArchived]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("archived"),
                QStringLiteral("true"), isArchived));
            applyFilter();
          });

          QAction *forkAction = menu.addAction(
              isFork ? i18n("Filter out forks") : i18n("Filter only forks"));
          connect(forkAction, &QAction::triggered, [this, isFork]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("fork"),
                QStringLiteral("true"), isFork));
            applyFilter();
          });

          QAction *privateAction =
              menu.addAction(isPrivate ? i18n("Filter out private")
                                       : i18n("Filter only private"));
          connect(privateAction, &QAction::triggered, [this, isPrivate]() {
            m_filterEdit->setText(FilterEditor::applyQuickFilter(
                m_filterEdit->text(), QStringLiteral("private"),
                QStringLiteral("true"), isPrivate));
            applyFilter();
          });
        }

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
  m_promptEdit = new PromptTextEdit(this);

  QHBoxLayout *promptHeaderLayout = new QHBoxLayout();
  m_loadTemplateButton = new QPushButton(tr("Load from template"), this);
  m_markdownModeComboBox = new QComboBox(this);
  m_markdownModeComboBox->addItem(tr("WYSIWYG Markdown"));
  m_markdownModeComboBox->addItem(tr("Raw Markdown"));

  KConfigGroup configPrompt(KSharedConfig::openConfig(),
                            QStringLiteral("NewSessionDialog"));
  int markdownMode = configPrompt.readEntry(QStringLiteral("MarkdownMode"), 0);
  if (markdownMode < 0 || markdownMode > 1) {
    markdownMode = 0;
  }
  m_markdownModeComboBox->setCurrentIndex(markdownMode);
  m_promptEdit->setMarkdownMode(markdownMode);
  connect(m_markdownModeComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged), m_promptEdit,
          &PromptTextEdit::setMarkdownMode);

  promptHeaderLayout->addWidget(m_loadTemplateButton);
  promptHeaderLayout->addStretch();
  promptHeaderLayout->addWidget(m_markdownModeComboBox);

  QVBoxLayout *promptLayout = new QVBoxLayout();
  promptLayout->addLayout(promptHeaderLayout);
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

  QAction *hideArchivedAction =
      actionCollection()->addAction(QStringLiteral("hide_archived_repos"));
  hideArchivedAction->setText(tr("Hide &Archived Repos"));
  hideArchivedAction->setCheckable(true);
  hideArchivedAction->setChecked(hideArchived);
  connect(hideArchivedAction, &QAction::toggled, this, [this](bool checked) {
    KConfigGroup config(KSharedConfig::openConfig(),
                        QStringLiteral("NewSessionDialog"));
    config.writeEntry(QStringLiteral("HideArchived"), checked);
    m_unselectedProxy->setHideArchived(checked);
  });

  QAction *hideForksAction =
      actionCollection()->addAction(QStringLiteral("hide_forks"));
  hideForksAction->setText(tr("Hide &Forks"));
  hideForksAction->setCheckable(true);
  hideForksAction->setChecked(hideForks);
  connect(hideForksAction, &QAction::toggled, this, [this](bool checked) {
    KConfigGroup config(KSharedConfig::openConfig(),
                        QStringLiteral("NewSessionDialog"));
    config.writeEntry(QStringLiteral("HideForks"), checked);
    m_unselectedProxy->setHideForks(checked);
  });

  QAction *hidePrivateAction =
      actionCollection()->addAction(QStringLiteral("hide_private_repos"));
  hidePrivateAction->setText(tr("Hide &Private Repos"));
  hidePrivateAction->setCheckable(true);
  hidePrivateAction->setChecked(hidePrivate);
  connect(hidePrivateAction, &QAction::toggled, this, [this](bool checked) {
    KConfigGroup config(KSharedConfig::openConfig(),
                        QStringLiteral("NewSessionDialog"));
    config.writeEntry(QStringLiteral("HidePrivate"), checked);
    m_unselectedProxy->setHidePrivate(checked);
  });

  QAction *hidePublicAction =
      actionCollection()->addAction(QStringLiteral("hide_public_repos"));
  hidePublicAction->setText(tr("Hide P&ublic Repos"));
  hidePublicAction->setCheckable(true);
  hidePublicAction->setChecked(hidePublic);
  connect(hidePublicAction, &QAction::toggled, this, [this](bool checked) {
    KConfigGroup config(KSharedConfig::openConfig(),
                        QStringLiteral("NewSessionDialog"));
    config.writeEntry(QStringLiteral("HidePublic"), checked);
    m_unselectedProxy->setHidePublic(checked);
  });

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

  m_promptEdit->setPromptText(prompt);
  updateModels();
}

void NewSessionDialog::setTemplateData(const QJsonObject &data) {
  QString prompt = data.value(QStringLiteral("prompt")).toString();
  m_promptEdit->setPromptText(prompt);

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
  QString text = m_filterEdit->text();
  QSharedPointer<ASTNode> ast;
  QString filterString;

  if (text.startsWith(QStringLiteral("="))) {
    ast = FilterParser::parse(text.mid(1));
  } else {
    filterString = text;
  }

  m_unselectedProxy->setFilterAST(ast);
  m_unselectedProxy->setFilterFixedString(filterString);

  bool applyToSelected = m_selectedSources.size() >= 10;
  m_selectedProxy->setFilterAST(applyToSelected ? ast
                                                : QSharedPointer<ASTNode>());
  m_selectedProxy->setFilterFixedString(applyToSelected ? filterString
                                                        : QStringLiteral(""));
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

  QMultiMap<QString, QString> sources = m_selectedSources;

  QString prompt = m_promptEdit->getPromptText();

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
  draft[QStringLiteral("prompt")] = m_promptEdit->getPromptText();
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
  tmpl[QStringLiteral("prompt")] = m_promptEdit->getPromptText();
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
  config.writeEntry(QStringLiteral("MarkdownMode"),
                    m_markdownModeComboBox->currentIndex());
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

  QJsonObject githubRepo =
      rawData.value(QStringLiteral("githubRepo")).toObject();
  if (githubRepo.contains(QStringLiteral("defaultBranch"))) {
    QJsonObject db =
        githubRepo.value(QStringLiteral("defaultBranch")).toObject();
    if (db.contains(QStringLiteral("displayName"))) {
      return db.value(QStringLiteral("displayName")).toString();
    }
  }

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

  auto addUniqueObjs = [&](const QJsonArray &arr) {
    for (const QJsonValue &v : arr) {
      QJsonObject obj = v.toObject();
      QString b = obj.value(QStringLiteral("displayName")).toString();
      if (!b.isEmpty() && !seen.contains(b)) {
        branches.append(b);
        seen.insert(b);
      }
    }
  };

  QJsonObject githubRepo =
      rawData.value(QStringLiteral("githubRepo")).toObject();
  addUniqueObjs(githubRepo.value(QStringLiteral("branches")).toArray());

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
  QMenu *favMenu = menu.addMenu(
      QIcon::fromTheme(QStringLiteral("emblem-favorite")), tr("Favourite"));
  QAction *toggleFavAction = favMenu->addAction(tr("Toggle Favourite"));
  connect(toggleFavAction, &QAction::triggered, this,
          [this, id]() { m_sourceModel->toggleFavourite(id); });
  QAction *incFavAction = favMenu->addAction(tr("Increase Rank"));
  connect(incFavAction, &QAction::triggered, this,
          [this, id]() { m_sourceModel->increaseFavouriteRank(id); });
  QAction *decFavAction = favMenu->addAction(tr("Decrease Rank"));
  connect(decFavAction, &QAction::triggered, this,
          [this, id]() { m_sourceModel->decreaseFavouriteRank(id); });
  QAction *setFavAction = favMenu->addAction(tr("Set Rank..."));
  connect(setFavAction, &QAction::triggered, this, [this, id, sourceIdx]() {
    bool ok;
    QVariant currentRankVal =
        m_sourceModel->data(sourceIdx, SourceModel::FavouriteRole);
    int initialRank = currentRankVal.isValid() ? currentRankVal.toInt() : 1;
    int rank = QInputDialog::getInt(this, tr("Set Favourite Rank"), tr("Rank:"),
                                    initialRank, 1, 10000, 1, &ok);
    if (ok) {
      m_sourceModel->setFavouriteRank(id, rank);
    }
  });
}
