#ifndef SESSIONMODEL_H
#define SESSIONMODEL_H

#include <QAbstractTableModel>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>
#include <optional>

struct SessionData {
  QString id;
  QString name;
  std::optional<int> favouriteRank;
  QString title;
  QString source;
  QString prompt;
  QString state;
  QDateTime updateTime;
  QDateTime createTime;
  QDateTime lastRefreshed;
  QString provider;
  QString owner;
  QString repo;
  bool hasChangeSet;
  QString prUrl;
  QString prNumber;
  QString prStatus;
  QStringList prLabels;
  QJsonObject rawObject;
  bool hasUnreadChanges = false;
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
    ProviderRole,
    LastRefreshedRole,
    PrStatusRole,
    PrLabelsRole,
    FavouriteRole,
    UnreadChangesRole
  };

  enum Columns {
    ColTitle = 0,
    ColFavourite,
    ColState,
    ColChangeSet,
    ColPR,
    ColPRStatus,
    ColPRLabels,
    ColUpdatedAt,
    ColCreatedAt,
    ColOwner,
    ColRepo,
    ColId,
    ColLastRefreshed,
    ColCount
  };

  explicit SessionModel(
      const QString &cacheFileName = QStringLiteral("cached_all_sessions.json"),
      QObject *parent = nullptr);

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
  void toggleFavourite(const QString &id);
  void setFavouriteRank(const QString &id, int rank);
  void increaseFavouriteRank(const QString &id);
  void decreaseFavouriteRank(const QString &id);
  void removeSession(int row);
  QJsonObject getSession(int row) const;
  QString getSessionName(const QString &id) const;
  QJsonArray getAllSessions() const;
  void clear();
  void loadSessions();
  void saveSessions();
  void clearSessions();
  void clearUnreadChanges();
  void markAsRead(const QString &id);
  bool contains(const QString &id) const;
  void setNextPageToken(const QString &token);
  QString nextPageToken() const;

  void clearAllUnreadChanges();
  void clearUnreadChanges(const QString &id);

Q_SIGNALS:
  void sessionsLoadedOrUpdated();

private:
  QVector<SessionData> m_sessions;
  QHash<QString, int> m_idToIndex;
  QString m_nextPageToken;
  QString m_cacheFileName;
};

#endif // SESSIONMODEL_H
