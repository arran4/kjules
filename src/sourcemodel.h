#ifndef SOURCEMODEL_H
#define SOURCEMODEL_H

#include <QAbstractTableModel>
#include <QJsonArray>
#include <QJsonObject>

class SourceModel : public QAbstractTableModel {
  Q_OBJECT

public:
  enum SourceRoles {
    NameRole = Qt::UserRole + 1,
    IdRole,
    RawDataRole,
    FavouriteRole
  };
  enum Columns {
    ColName = 0,
    ColFavourite,
    ColLastUsed,
    ColManagedSessions,
    ColHeat,
    ColFirstSeen,
    ColLastChanged,
    ColDescription,
    ColArchived,
    ColFork,
    ColPrivate,
    ColLanguages,
    ColCount
  };

  explicit SourceModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setSources(const QJsonArray &sources);
  int addSources(const QJsonArray &sources);
  void loadSources();
  void saveSources();
  void updateSource(const QJsonObject &sourceConst);
  void toggleFavourite(const QString &id);
  void clear();
  void recordSessionCreated(const QString &sourceId);
  void recalculateStatsFromSessions(const QJsonArray &allSessions);

private:
  QJsonArray m_sources;
};

#endif // SOURCEMODEL_H
