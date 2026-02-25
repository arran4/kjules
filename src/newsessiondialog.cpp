#include "newsessiondialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QListView>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QDebug>
#include <QJsonArray>

NewSessionDialog::NewSessionDialog(SourceModel *sourceModel, QWidget *parent)
    : QDialog(parent), m_sourceModel(sourceModel)
{
    setWindowTitle(tr("Create New Session"));
    resize(700, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QFormLayout *formLayout = new QFormLayout();

    // Source Selection
    QVBoxLayout *sourceLayout = new QVBoxLayout();

    QHBoxLayout *filterLayout = new QHBoxLayout();
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter sources..."));
    filterLayout->addWidget(m_filterEdit);

    QPushButton *selectAllBtn = new QPushButton(tr("Select All"), this);
    connect(selectAllBtn, &QPushButton::clicked, this, &NewSessionDialog::onSelectAll);

    QPushButton *unselectAllBtn = new QPushButton(tr("Unselect All"), this);
    connect(unselectAllBtn, &QPushButton::clicked, this, &NewSessionDialog::onUnselectAll);

    filterLayout->addWidget(selectAllBtn);
    filterLayout->addWidget(unselectAllBtn);

    sourceLayout->addLayout(filterLayout);

    m_sourceView = new QListView(this);

    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(m_sourceModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterRole(SourceModel::NameRole);

    connect(m_filterEdit, &QLineEdit::textChanged, proxyModel, &QSortFilterProxyModel::setFilterFixedString);

    m_sourceView->setModel(proxyModel);
    m_sourceView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_sourceView->setFixedHeight(200);
    sourceLayout->addWidget(m_sourceView);

    formLayout->addRow(tr("Sources:"), sourceLayout);

    // Prompt
    m_promptEdit = new QTextEdit(this);
    formLayout->addRow(tr("Prompt:"), m_promptEdit);

    // Automation Mode
    m_automationModeCombo = new QComboBox(this);
    m_automationModeCombo->addItem(tr("None"), "");
    m_automationModeCombo->addItem(tr("Auto Create PR"), "AUTO_CREATE_PR");
    formLayout->addRow(tr("Automation Mode:"), m_automationModeCombo);

    mainLayout->addLayout(formLayout);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *draftButton = new QPushButton(tr("Save as Draft"), this);
    connect(draftButton, &QPushButton::clicked, this, &NewSessionDialog::onSaveDraft);

    QPushButton *createButton = new QPushButton(tr("Create Session"), this);
    createButton->setDefault(true);
    connect(createButton, &QPushButton::clicked, this, &NewSessionDialog::onSubmit);

    QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addWidget(draftButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(createButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void NewSessionDialog::setInitialData(const QJsonObject &data)
{
    QString prompt = data.value("prompt").toString();
    QString automationMode = data.value("automationMode").toString();

    // Check for "sources" array, fallback to "source" string
    QStringList sources;
    if (data.contains("sources")) {
        QJsonArray arr = data.value("sources").toArray();
        for(const auto &val : arr) {
            sources.append(val.toString());
        }
    } else if (data.contains("source")) {
        sources.append(data.value("source").toString());
    }

    m_promptEdit->setPlainText(prompt);
    int index = m_automationModeCombo->findData(automationMode);
    if (index != -1) {
        m_automationModeCombo->setCurrentIndex(index);
    }

    // Select sources
    QAbstractItemModel *model = m_sourceView->model(); // Proxy model
    QItemSelectionModel *selectionModel = m_sourceView->selectionModel();
    selectionModel->clearSelection();

    for(int i = 0; i < model->rowCount(); ++i) {
        QModelIndex idx = model->index(i, 0);
        // We need to map role data. Proxy usually forwards data.
        QString id = model->data(idx, SourceModel::IdRole).toString();
        // Or NameRole if that's what we use for API.
        // Let's assume we use IdRole to match with stored sources.
        // Wait, earlier I decided to use NameRole (resource path) for API.
        // So sources list should contain names.
        // Let's use NameRole.
        QString name = model->data(idx, SourceModel::NameRole).toString();

        if (sources.contains(name)) {
             selectionModel->select(idx, QItemSelectionModel::Select);
        }
    }
}

void NewSessionDialog::onSubmit()
{
    QModelIndexList selection = m_sourceView->selectionModel()->selectedIndexes();
    if (selection.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Source"), tr("Please select at least one source."));
        return;
    }

    QStringList sources;
    for(const QModelIndex &idx : selection) {
        sources.append(idx.data(SourceModel::NameRole).toString());
    }

    QString prompt = m_promptEdit->toPlainText();
    QString automationMode = m_automationModeCombo->currentData().toString();

    if (prompt.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Prompt"), tr("Please enter a prompt."));
        return;
    }

    emit createSessionRequested(sources, prompt, automationMode);
    accept();
}

void NewSessionDialog::onSaveDraft()
{
    QModelIndexList selection = m_sourceView->selectionModel()->selectedIndexes();
    QJsonArray sourcesArr;
    for(const QModelIndex &idx : selection) {
        sourcesArr.append(idx.data(SourceModel::NameRole).toString());
    }

    QString prompt = m_promptEdit->toPlainText();
    QString automationMode = m_automationModeCombo->currentData().toString();

    QJsonObject draft;
    draft["sources"] = sourcesArr;
    draft["prompt"] = prompt;
    draft["automationMode"] = automationMode;

    emit saveDraftRequested(draft);
    accept();
}

void NewSessionDialog::onSelectAll()
{
    m_sourceView->selectAll();
}

void NewSessionDialog::onUnselectAll()
{
    m_sourceView->clearSelection();
}
