#ifndef SOURCEMODEL_H
#define SOURCEMODEL_H

#include <QAbstractTableModel>
#include <QJsonArray>

class SourceModel : public QAbstractTableModel {
  Q_OBJECT

public:
  enum SourceRoles {
    NameRole = Qt::UserRole + 1,
    IdRole,
    IsNewRole,
    TypeRole,
    DescriptionRole,
    LastUpdatedRole
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

private:
  QJsonArray m_sources;
  QStringList m_knownSourceIds;
};

#endif // SOURCEMODEL_H
