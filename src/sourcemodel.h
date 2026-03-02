#ifndef SOURCEMODEL_H
#define SOURCEMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>

class SourceModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum SourceRoles { NameRole = Qt::UserRole + 1, IdRole };

  explicit SourceModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setSources(const QJsonArray &sources);

private:
  QJsonArray m_sources;
};

#endif // SOURCEMODEL_H
