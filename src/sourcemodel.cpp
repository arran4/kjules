#include "sourcemodel.h"
#include <QJsonObject>

SourceModel::SourceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SourceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_sources.size();
}

QVariant SourceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_sources.size())
        return QVariant();

    const QJsonObject source = m_sources[index.row()].toObject();

    switch (role) {
    case NameRole:
        return source.value(QStringLiteral("name")).toString();
    case IdRole:
        return source.value(QStringLiteral("id")).toString();
    case Qt::DisplayRole:
        return source.value(QStringLiteral("name")).toString();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SourceModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[IdRole] = "id";
    return roles;
}

void SourceModel::setSources(const QJsonArray &sources)
{
    beginResetModel();
    m_sources = sources;
    endResetModel();
}
