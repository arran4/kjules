#ifndef SESSIONMODEL_H
#define SESSIONMODEL_H

#include <QAbstractTableModel>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>
#include <QDateTime>

struct SessionData {
  QString id;
  QString name;
  QString title;
  QString source;
  QString prompt;
  QString state;
  QDateTime updateTime;
  QDateTime createTime;
  QString provider;
  QString owner;
  QString repo;
  bool hasChangeSet;
  QString prUrl;
  QString prNumber;
  QJsonObject rawObject;
};

class SessionModel : public QAbstractTableModel {
  Q_OBJECT

public:
  enum SessionRoles {
    IdRole = Qt::UserRole + 1,
    NameRole,
    TitleRole,
    SourceRole,
    PromptRole,
    StateRole,
    ChangeSetRole,
    PrUrlRole,
    ProviderRole
  };

  enum Columns {
    ColTitle = 0,
    ColState,
    ColChangeSet,
    ColPR,
    ColUpdatedAt,
    ColCreatedAt,
    ColOwner,
    ColRepo,
    ColId,
    ColCount
  };

  explicit SessionModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setSessions(const QJsonArray &sessions);
  int addSessions(const QJsonArray &sessions);
  void addSession(const QJsonObject &session);
  void updateSession(const QJsonObject &session);
  QJsonObject getSession(int row) const;
  void clear();
  void loadSessions();
  void saveSessions();
  void clearSessions();
  void setNextPageToken(const QString &token);
  QString nextPageToken() const;

private:
  QVector<SessionData> m_sessions;
  QHash<QString, int> m_idToIndex;
  QString m_nextPageToken;
};

#endif // SESSIONMODEL_H
