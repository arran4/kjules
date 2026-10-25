#include "draftsmodel.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

DraftsModel::DraftsModel(QObject *parent) : QAbstractListModel(parent) { loadDrafts(); }

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
      QJsonArray sourcesArray = draft.value(QStringLiteral("sources")).toArray();
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
  case CommentRole:
    return draft.value(QStringLiteral("comment")).toString();
  case Qt::DisplayRole: {
    QString comment = draft.value(QStringLiteral("comment")).toString();
    if (!comment.isEmpty())
      return comment;
    return draft.value(QStringLiteral("prompt")).toString(); // Fallback
  }
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

bool DraftsModel::addDraft(const QJsonObject &draft) {
  beginInsertRows(QModelIndex(), 0, 0);
  m_drafts.insert(0, draft);
  endInsertRows();
  if (!saveDrafts()) {
    beginRemoveRows(QModelIndex(), 0, 0);
    m_drafts.removeAt(0);
    endRemoveRows();
    return false;
  }
  return true;
}

bool DraftsModel::updateDraft(int row, const QJsonObject &draft) {
  if (row < 0 || row >= m_drafts.size()) {
    return false;
  }
  QJsonObject backup = m_drafts[row].toObject();
  m_drafts[row] = draft;
  Q_EMIT dataChanged(index(row, 0), index(row, 0));
  if (!saveDrafts()) {
    m_drafts[row] = backup;
    Q_EMIT dataChanged(index(row, 0), index(row, 0));
    return false;
  }
  return true;
}

void DraftsModel::clear() {
  beginResetModel();
  m_drafts = QJsonArray();
  endResetModel();
}

bool DraftsModel::removeDraft(int row) {
  if (row >= 0 && row < m_drafts.size()) {
    QJsonValue removedItem = m_drafts.at(row);
    beginRemoveRows(QModelIndex(), row, row);
    m_drafts.removeAt(row);
    endRemoveRows();
    if (!saveDrafts()) {
      beginInsertRows(QModelIndex(), row, row);
      m_drafts.insert(row, removedItem);
      endInsertRows();
      return false;
    }
    return true;
  }
  return false;
}

QJsonObject DraftsModel::getDraft(int row) const {
  if (row >= 0 && row < m_drafts.size()) {
    return m_drafts[row].toObject();
  }
  return QJsonObject();
}

void DraftsModel::loadDrafts() {
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QFile file(path + QStringLiteral("/drafts.json"));
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_drafts = doc.array();
    file.close();
  }
}

bool DraftsModel::saveDrafts() {
  if (m_simulateSaveFailure) {
    return false;
  }
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(path);
  if (!dir.exists()) {
    if (!dir.mkpath(QStringLiteral("."))) {
      return false;
    }
  }
  QSaveFile file(path + QStringLiteral("/drafts.json"));
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  QJsonDocument doc(m_drafts);
  QByteArray data = doc.toJson();
  qint64 written = file.write(data);
  if (written != data.size()) {
    file.cancelWriting();
    return false;
  }
  return file.commit();
}
