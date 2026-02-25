#include "sessionmodel.h"
#include <QDebug>

SessionModel::SessionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SessionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_sessions.size();
}

QVariant SessionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_sessions.size())
        return QVariant();

    const QJsonObject session = m_sessions[index.row()].toObject();

    switch (role) {
    case IdRole:
        return session.value(QStringLiteral("id")).toString();
    case NameRole:
        return session.value(QStringLiteral("name")).toString();
    case TitleRole:
        return session.value(QStringLiteral("title")).toString();
    case SourceRole:
        return session.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString();
    case PromptRole:
        return session.value(QStringLiteral("prompt")).toString();
    case Qt::DisplayRole:
        return session.value(QStringLiteral("title")).toString();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SessionModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[NameRole] = "name";
    roles[TitleRole] = "title";
    roles[SourceRole] = "source";
    roles[PromptRole] = "prompt";
    return roles;
}

void SessionModel::setSessions(const QJsonArray &sessions)
{
    beginResetModel();
    m_sessions = sessions;
    endResetModel();
}

void SessionModel::addSession(const QJsonObject &session)
{
    beginInsertRows(QModelIndex(), 0, 0);
    m_sessions.insert(0, session);
    endInsertRows();
}

void SessionModel::updateSession(const QJsonObject &session)
{
    QString id = session.value(QStringLiteral("id")).toString();
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].toObject().value(QStringLiteral("id")).toString() == id) {
            m_sessions[i] = session;
            Q_EMIT dataChanged(index(i, 0), index(i, 0));
            return;
        }
    }
    // If not found, add it
    addSession(session);
}

QJsonObject SessionModel::getSession(int row) const
{
    if (row >= 0 && row < m_sessions.size()) {
        return m_sessions[row].toObject();
    }
    return QJsonObject();
}
