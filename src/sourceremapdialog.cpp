#include "sourceremapdialog.h"
#include "sourcefixer.h"

#include <KLocalizedString>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

SourceRemapDialog::SourceRemapDialog(const QList<SourceRemapEntry> &entries, const QStringList &availableSources,
                                     QWidget *parent)
    : QDialog(parent), m_entries(entries), m_table(new QTableWidget(entries.size(), 6, this)) {
  setWindowTitle(i18n("Fix Sources"));
  resize(1000, 500);
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(i18n("Choose replacement source IDs. Best matches are preselected; uncheck rows you "
                                    "do not want to change. Right-click a row to view its full details."),
                               this));
  m_table->setHorizontalHeaderLabels({i18n("Apply"), i18n("Status"), i18n("Location"), i18n("Task"),
                                      i18n("Current Source"), i18n("Replacement Source")});
  m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
  m_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

  for (int row = 0; row < entries.size(); ++row) {
    auto *enabled = new QTableWidgetItem();
    enabled->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    enabled->setCheckState(!availableSources.isEmpty() && entries[row].selectedByDefault ? Qt::Checked : Qt::Unchecked);
    enabled->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 0, enabled);
    m_table->setItem(row, 1, new QTableWidgetItem(entries[row].status));
    m_table->setItem(row, 2, new QTableWidgetItem(entries[row].location));
    m_table->setItem(row, 3, new QTableWidgetItem(entries[row].description));
    m_table->setItem(row, 4, new QTableWidgetItem(entries[row].source));
    auto *replacement = new QComboBox(m_table);
    replacement->addItems(availableSources);
    replacement->setCurrentText(SourceFixer::bestMatch(entries[row].source, availableSources));
    m_table->setCellWidget(row, 5, replacement);
  }
  layout->addWidget(m_table);

  auto *selectionButtons = new QHBoxLayout();
  auto *selectAll = new QPushButton(i18n("Select All"), this);
  auto *selectNone = new QPushButton(i18n("Select None"), this);
  connect(selectAll, &QPushButton::clicked, this, [this]() {
    for (int row = 0; row < m_table->rowCount(); ++row) {
      m_table->item(row, 0)->setCheckState(Qt::Checked);
    }
  });
  connect(selectNone, &QPushButton::clicked, this, [this]() {
    for (int row = 0; row < m_table->rowCount(); ++row) {
      m_table->item(row, 0)->setCheckState(Qt::Unchecked);
    }
  });
  selectionButtons->addWidget(selectAll);
  selectionButtons->addWidget(selectNone);
  selectionButtons->addStretch();
  layout->addLayout(selectionButtons);

  connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &position) {
    const int row = m_table->rowAt(position.y());
    if (row < 0 || row >= m_entries.size()) {
      return;
    }
    m_table->selectRow(row);
    QMenu menu(this);
    QAction *detailsAction = menu.addAction(i18n("View Details..."));
    if (menu.exec(m_table->viewport()->mapToGlobal(position)) != detailsAction) {
      return;
    }
    QDialog details(this);
    details.setWindowTitle(i18n("Source Reference Details"));
    details.resize(750, 550);
    auto *detailsLayout = new QVBoxLayout(&details);
    detailsLayout->addWidget(
        new QLabel(i18n("%1 — %2", m_entries[row].location, m_entries[row].description), &details));
    auto *rawData = new QTextEdit(&details);
    rawData->setReadOnly(true);
    rawData->setPlainText(QString::fromUtf8(QJsonDocument(m_entries[row].data).toJson(QJsonDocument::Indented)));
    detailsLayout->addWidget(rawData);
    auto *closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, &details);
    connect(closeButtons, &QDialogButtonBox::rejected, &details, &QDialog::reject);
    detailsLayout->addWidget(closeButtons);
    details.exec();
  });

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
  QPushButton *applyButton = buttons->button(QDialogButtonBox::Apply);
  applyButton->setText(i18n("Apply Checked"));
  connect(applyButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
}

QList<SourceRemapSelection> SourceRemapDialog::selections() const {
  QList<SourceRemapSelection> result;
  for (int row = 0; row < m_entries.size(); ++row) {
    const QTableWidgetItem *enabled = m_table->item(row, 0);
    const auto *replacement = qobject_cast<QComboBox *>(m_table->cellWidget(row, 5));
    if (enabled && enabled->checkState() == Qt::Checked && replacement && !replacement->currentText().isEmpty() &&
        replacement->currentText() != m_entries[row].source) {
      result.append({m_entries[row], replacement->currentText()});
    }
  }
  return result;
}
