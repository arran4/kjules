#include "draftsmodel.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

DraftsModel::DraftsModel(QObject *parent) : QAbstractListModel(parent) {
  loadDrafts();
}

int DraftsModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_drafts.size();
}

QVariant DraftsModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_drafts.size())
    return QVariant();

  const QJsonObject draft = m_drafts[index.row()].toObject();

  switch (role) {
  case SourceRole: {
    if (draft.contains(QStringLiteral("sources"))) {
      QJsonArray sourcesArray =
          draft.value(QStringLiteral("sources")).toArray();
      QStringList sourcesList;
      for (const QJsonValue &val : sourcesArray) {
        sourcesList.append(val.toString());
      }
      return sourcesList.join(QStringLiteral(", "));
    }
    return draft.value(QStringLiteral("source")).toString();
  }
  case PromptRole:
    return draft.value(QStringLiteral("prompt")).toString();
  case AutomationModeRole:
    return draft.value(QStringLiteral("automationMode")).toString();
  case Qt::DisplayRole:
    return draft.value(QStringLiteral("prompt"))
        .toString(); // Display prompt as title
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> DraftsModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[SourceRole] = "source";
  roles[PromptRole] = "prompt";
  roles[AutomationModeRole] = "automationMode";
  return roles;
}

void DraftsModel::addDraft(const QJsonObject &draft) {
  beginInsertRows(QModelIndex(), 0, 0);
  m_drafts.insert(0, draft);
  endInsertRows();
  saveDrafts();
}

void DraftsModel::clear() {
  beginResetModel();
  m_drafts = QJsonArray();
  endResetModel();
}

void DraftsModel::removeDraft(int row) {
  if (row >= 0 && row < m_drafts.size()) {
    beginRemoveRows(QModelIndex(), row, row);
    m_drafts.removeAt(row);
    endRemoveRows();
    saveDrafts();
  }
}

QJsonObject DraftsModel::getDraft(int row) const {
  if (row >= 0 && row < m_drafts.size()) {
    return m_drafts[row].toObject();
  }
  return QJsonObject();
}

void DraftsModel::loadDrafts() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QFile file(path + QStringLiteral("/drafts.json"));
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_drafts = doc.array();
    file.close();
  }
}

void DraftsModel::saveDrafts() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(path);
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }
  QFile file(path + QStringLiteral("/drafts.json"));
  if (file.open(QIODevice::WriteOnly)) {
    QJsonDocument doc(m_drafts);
    file.write(doc.toJson());
    file.close();
  }
}
