#include "newsessiondialog.h"
#include "savedialog.h"
#include "templateselectiondialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QTextEdit>
#include <QVBoxLayout>

class SourceSelectionProxyModel : public QSortFilterProxyModel {
public:
  SourceSelectionProxyModel(const QSet<QString> *selectedSources,
                            bool showSelected, QObject *parent = nullptr)
      : QSortFilterProxyModel(parent), m_selectedSources(selectedSources),
        m_showSelected(showSelected) {}

  void updateSelection() { invalidateFilter(); }

protected:
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
  const QSet<QString> *m_selectedSources;
  bool m_showSelected;
};

NewSessionDialog::NewSessionDialog(SourceModel *sourceModel, TemplatesModel *templatesModel, bool hasApiKey,
                                   QWidget *parent)
    : QDialog(parent), m_sourceModel(sourceModel), m_templatesModel(templatesModel) {
  setWindowTitle(tr("Create New Session"));
  resize(700, 600);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  QFormLayout *formLayout = new QFormLayout();

  // Source Selection
  QVBoxLayout *sourceLayout = new QVBoxLayout();

  m_filterEdit = new QLineEdit(this);
  m_filterEdit->setPlaceholderText(tr("Filter sources..."));
  sourceLayout->addWidget(m_filterEdit);

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
  m_unselectedView->setModel(m_unselectedProxy);
  m_unselectedView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_unselectedView->setFixedHeight(200);
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
  m_selectedView->setModel(m_selectedProxy);
  m_selectedView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_selectedView->setFixedHeight(200);
  selectedLayout->addWidget(m_selectedView);

  splitViewLayout->addLayout(unselectedLayout);
  splitViewLayout->addLayout(middleButtonsLayout);
  splitViewLayout->addLayout(selectedLayout);

  sourceLayout->addLayout(splitViewLayout);

  connect(m_filterEdit, &QLineEdit::textChanged, m_unselectedProxy,
          &QSortFilterProxyModel::setFilterFixedString);
  connect(m_filterEdit, &QLineEdit::textChanged, m_selectedProxy,
          &QSortFilterProxyModel::setFilterFixedString);

  connect(m_unselectedView, &QListView::doubleClicked, this,
          [this](const QModelIndex &idx) {
            m_selectedSources.insert(
                idx.data(SourceModel::NameRole).toString());
            updateModels();
            m_unselectedView->clearSelection();
          });

  connect(m_selectedView, &QListView::doubleClicked, this,
          [this](const QModelIndex &idx) {
            m_selectedSources.remove(
                idx.data(SourceModel::NameRole).toString());
            updateModels();
            m_selectedView->clearSelection();
          });

  formLayout->addRow(tr("Sources:"), sourceLayout);

  // Prompt
  m_promptEdit = new QTextEdit(this);
  m_loadTemplateButton = new QPushButton(tr("Load from template"), this);
  connect(m_loadTemplateButton, &QPushButton::clicked, this, [this]() {
    TemplateSelectionDialog dlg(m_templatesModel, this);
    if (dlg.exec() == QDialog::Accepted) {
      setTemplateData(dlg.selectedTemplate());
    }
  });

  QVBoxLayout *promptLayout = new QVBoxLayout();
  promptLayout->addWidget(m_loadTemplateButton, 0, Qt::AlignLeft);
  promptLayout->addWidget(m_promptEdit);

  connect(m_promptEdit, &QTextEdit::textChanged, this, [this]() {
    m_loadTemplateButton->setVisible(m_promptEdit->toPlainText().trimmed().isEmpty());
  });

  formLayout->addRow(tr("Prompt:"), promptLayout);

  // Require Plan Approval
  m_requirePlanApprovalCheckBox = new QCheckBox(this);
  m_requirePlanApprovalCheckBox->setChecked(false);
  formLayout->addRow(tr("Require Plan Approval:"), m_requirePlanApprovalCheckBox);

  mainLayout->addLayout(formLayout);

  // Buttons
  QHBoxLayout *buttonLayout = new QHBoxLayout();

  QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  QPushButton *draftButton = new QPushButton(tr("Save as Draft"), this);
  connect(draftButton, &QPushButton::clicked, this,
          &NewSessionDialog::onSaveDraft);

  m_saveTemplateButton = new QPushButton(tr("Save as Template"), this);
  connect(m_saveTemplateButton, &QPushButton::clicked, this,
          &NewSessionDialog::onSaveTemplate);

  m_createButton = new QPushButton(tr("Create Session"), this);

  if (!hasApiKey) {
    m_createButton->setEnabled(false);
    m_createButton->setToolTip(
        tr("An API key is required to create a session."));
  }
  connect(m_createButton, &QPushButton::clicked, this,
          [this]() { onSubmit(QStringLiteral("")); });

  m_createPRButton = new QPushButton(tr("Create PR Session"), this);
  m_createPRButton->setDefault(true);

  if (!hasApiKey) {
    m_createPRButton->setEnabled(false);
    m_createPRButton->setToolTip(
        tr("An API key is required to create a session."));
  }
  connect(m_createPRButton, &QPushButton::clicked, this,
          [this]() { onSubmit(QStringLiteral("AUTO_CREATE_PR")); });

  buttonLayout->addWidget(cancelButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(draftButton);
  buttonLayout->addWidget(m_saveTemplateButton);
  buttonLayout->addWidget(m_createButton);
  buttonLayout->addWidget(m_createPRButton);

  mainLayout->addLayout(buttonLayout);
}

void NewSessionDialog::setEditMode(bool isEdit) {
  if (isEdit) {
    setWindowTitle(tr("Edit Queued Session"));
    m_createButton->setText(tr("Requeue Session"));
    m_createPRButton->setText(tr("Requeue PR Session"));
  } else {
    setWindowTitle(tr("Create New Session"));
    m_createButton->setText(tr("Create Session"));
    m_createPRButton->setText(tr("Create PR Session"));
  }
}

  void NewSessionDialog::setInitialData(const QJsonObject &data) {
  QString prompt = data.value(QStringLiteral("prompt")).toString();

  if (data.contains(QStringLiteral("requirePlanApproval"))) {
    m_requirePlanApprovalCheckBox->setChecked(data.value(QStringLiteral("requirePlanApproval")).toBool());
  }

  if (data.contains(QStringLiteral("comment"))) {
    m_draftComment = data.value(QStringLiteral("comment")).toString();
  } else {
    m_draftComment.clear();
  }

  // Check for "sources" array, fallback to "source" string
  QStringList sources;
  if (data.contains(QStringLiteral("sources"))) {
    QJsonArray arr = data.value(QStringLiteral("sources")).toArray();
    for (const auto &val : arr) {
      sources.append(val.toString());
    }
  } else if (data.contains(QStringLiteral("source"))) {
    sources.append(data.value(QStringLiteral("source")).toString());
  } else if (data.contains(QStringLiteral("sourceContext"))) {
    QJsonObject sc = data.value(QStringLiteral("sourceContext")).toObject();
    if (sc.contains(QStringLiteral("source"))) {
      QString s = sc.value(QStringLiteral("source")).toString();
      if (s.startsWith(QStringLiteral("sources/"))) {
        s = s.mid(8); // Remove "sources/" prefix to match the NameRole in SourceModel
      }
      sources.append(s);
    }
  }

  m_promptEdit->setPlainText(prompt);

  m_selectedSources.clear();
  for (const QString &src : sources) {
    m_selectedSources.insert(src);
  }
  updateModels();
}

void NewSessionDialog::setTemplateData(const QJsonObject &data) {
  QString prompt = data.value(QStringLiteral("prompt")).toString();
  m_promptEdit->setPlainText(prompt);

  if (data.contains(QStringLiteral("requirePlanApproval"))) {
    m_requirePlanApprovalCheckBox->setChecked(data.value(QStringLiteral("requirePlanApproval")).toBool());
  }
}

void NewSessionDialog::updateModels() {
  m_unselectedProxy->updateSelection();
  m_selectedProxy->updateSelection();
}

void NewSessionDialog::onAddSelected() {
  QModelIndexList selection =
      m_unselectedView->selectionModel()->selectedIndexes();
  for (const QModelIndex &idx : selection) {
    m_selectedSources.insert(idx.data(SourceModel::NameRole).toString());
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
    m_selectedSources.insert(idx.data(SourceModel::NameRole).toString());
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

  QStringList sources(m_selectedSources.begin(), m_selectedSources.end());
  sources.sort();

  QString prompt = m_promptEdit->toPlainText();

  if (prompt.isEmpty()) {
    QMessageBox::warning(this, tr("Missing Prompt"),
                         tr("Please enter a prompt."));
    return;
  }

  bool requirePlanApproval = m_requirePlanApprovalCheckBox->isChecked();

  Q_EMIT createSessionRequested(sources, prompt, automationMode, requirePlanApproval);
  accept();
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
  QStringList sources(m_selectedSources.begin(), m_selectedSources.end());
  sources.sort();
  for (const QString &src : sources) {
    sourcesArr.append(src);
  }

  QString prompt = m_promptEdit->toPlainText();
  bool requirePlanApproval = m_requirePlanApprovalCheckBox->isChecked();

  QJsonObject draft;
  draft[QStringLiteral("sources")] = sourcesArr;
  draft[QStringLiteral("prompt")] = prompt;
  draft[QStringLiteral("comment")] = dlg.nameOrComment();
  draft[QStringLiteral("requirePlanApproval")] = requirePlanApproval;

  Q_EMIT saveDraftRequested(draft);
  accept();
}

void NewSessionDialog::onSaveTemplate() {
  SaveDialog dlg(QStringLiteral("Template"), this);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  QJsonArray sourcesArr;
  QStringList sources(m_selectedSources.begin(), m_selectedSources.end());
  sources.sort();
  for (const QString &src : sources) {
    sourcesArr.append(src);
  }

  QString prompt = m_promptEdit->toPlainText();
  bool requirePlanApproval = m_requirePlanApprovalCheckBox->isChecked();

  QJsonObject tmpl;
  // A template might not necessarily need sources but we save them anyway
  // as the requirement "a template doesn't set the sources in an existing new session window"
  // means we ignore sources when loading in an *existing* window.
  tmpl[QStringLiteral("sources")] = sourcesArr;
  tmpl[QStringLiteral("prompt")] = prompt;
  tmpl[QStringLiteral("name")] = dlg.nameOrComment();
  tmpl[QStringLiteral("description")] = dlg.description();
  tmpl[QStringLiteral("requirePlanApproval")] = requirePlanApproval;

  Q_EMIT saveTemplateRequested(tmpl);
  // We do not close the dialog when saving a template, it can be used multiple times
}
