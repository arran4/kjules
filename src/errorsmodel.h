#ifndef ERRORSMODEL_H
#define ERRORSMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>

class ErrorsModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum ErrorRoles {
    RequestRole = Qt::UserRole + 1,
    ResponseRole,
    MessageRole,
    HttpDetailsRole,
    TimestampRole
  };

  explicit ErrorsModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  /**
   * @brief Adds an error object to the model.
   *
   * The expected JSON structure for errorObj is:
   * {
   *   "request": QJsonObject,     // The request payload/details (optional but
   * expected) "response": QJsonObject,    // The response payload/details
   * (optional but expected) "message": QString,         // A human-readable
   * error message (used as the title) "httpDetails": QString,     // (Optional)
   * Detailed HTTP status/headers "timestamp": QString        // (Optional) ISO
   * 8601 formatted UTC timestamp
   * }
   */
  void addErrorObj(const QJsonObject &errorObj);
  void removeError(int row);
  QJsonObject getError(int row) const;
  void loadErrors();
  void saveErrors();
  void clear();

private:
  QJsonArray m_errors;
};

#endif // ERRORSMODEL_H
