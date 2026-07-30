import re

with open("src/advancedfilterproxymodel.cpp", "r") as f:
    code = f.read()

# Add includes
code = code.replace('#include <QStringList>', '#include <QStringList>\n#include <QJsonObject>\n#include <QJsonArray>')

# Add public mapping
code = code.replace('{QStringLiteral("private"), SourceModel::ColPrivate},', '{QStringLiteral("private"), SourceModel::ColPrivate},\n                                                    {QStringLiteral("public"), SourceModel::ColPrivate},')

# Get Bool Str lambda
get_bool_str = """
    auto getBoolStr = [](const QJsonObject &rawData, const QString &k) {
        QJsonObject github = rawData.value(QStringLiteral("github")).toObject();
        if (k == QStringLiteral("private")) {
            bool isPrivate = rawData.contains(QStringLiteral("isPrivate")) ? rawData.value(QStringLiteral("isPrivate")).toBool() : github.value(QStringLiteral("private")).toBool();
            return isPrivate ? QStringLiteral("true") : QStringLiteral("false");
        } else if (k == QStringLiteral("public")) {
            bool isPrivate = rawData.contains(QStringLiteral("isPrivate")) ? rawData.value(QStringLiteral("isPrivate")).toBool() : github.value(QStringLiteral("private")).toBool();
            return !isPrivate ? QStringLiteral("true") : QStringLiteral("false");
        } else if (k == QStringLiteral("fork")) {
            bool isFork = rawData.contains(QStringLiteral("isFork")) ? rawData.value(QStringLiteral("isFork")).toBool() : github.value(QStringLiteral("fork")).toBool();
            return isFork ? QStringLiteral("true") : QStringLiteral("false");
        } else if (k == QStringLiteral("archived")) {
            bool isArchived = rawData.contains(QStringLiteral("isArchived")) ? rawData.value(QStringLiteral("isArchived")).toBool() : github.value(QStringLiteral("archived")).toBool();
            return isArchived ? QStringLiteral("true") : QStringLiteral("false");
        }
        return QString();
    };
"""

code = code.replace('    if (qobject_cast<SourceModel *>(model) && keyToColumn.contains(lowerKey)) {', get_bool_str + '\n    if (qobject_cast<SourceModel *>(model) && keyToColumn.contains(lowerKey)) {')

# Modify SourceModel block
source_mod = """    if (qobject_cast<SourceModel *>(model) && keyToColumn.contains(lowerKey)) {
      if (lowerKey == QStringLiteral("private") || lowerKey == QStringLiteral("public") || lowerKey == QStringLiteral("fork") || lowerKey == QStringLiteral("archived")) {
          QJsonObject rawData = model->data(model->index(row, 0, parent), SourceModel::RawDataRole).toJsonObject();
          return getBoolStr(rawData, lowerKey);
      }
      return model->data(model->index(row, keyToColumn.value(lowerKey), parent), Qt::DisplayRole).toString();"""
code = code.replace('    if (qobject_cast<SourceModel *>(model) && keyToColumn.contains(lowerKey)) {\n      return model->data(model->index(row, keyToColumn.value(lowerKey), parent), Qt::DisplayRole).toString();', source_mod)

# Modify SessionModel block
session_mod = """      if (!matches.isEmpty()) {
        QModelIndex sourceIdx = matches.first();
        if (lowerKey == QStringLiteral("private") || lowerKey == QStringLiteral("public") || lowerKey == QStringLiteral("fork") || lowerKey == QStringLiteral("archived")) {
            QJsonObject rawData = globalSourceModel->data(globalSourceModel->index(sourceIdx.row(), 0), SourceModel::RawDataRole).toJsonObject();
            return getBoolStr(rawData, lowerKey);
        }
        return globalSourceModel
            ->data(globalSourceModel->index(sourceIdx.row(), keyToColumn.value(lowerKey)), Qt::DisplayRole)
            .toString();"""
code = code.replace('      if (!matches.isEmpty()) {\n        QModelIndex sourceIdx = matches.first();\n        return globalSourceModel\n            ->data(globalSourceModel->index(sourceIdx.row(), keyToColumn.value(lowerKey)), Qt::DisplayRole)\n            .toString();', session_mod)

with open("src/advancedfilterproxymodel.cpp", "w") as f:
    f.write(code)
