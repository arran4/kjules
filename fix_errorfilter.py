import re

with open('src/sourcestatuswidget.cpp', 'r') as f:
    content = f.read()

impl = '''bool ErrorFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  if (!sourceModel()) {
    return false;
  }
  QModelIndex index = sourceModel()->index(source_row, 0, source_parent);

  // Use SourceIdRole first
  QString explicitSourceId = sourceModel()->data(index, ErrorsModel::SourceIdRole).toString();
  if (!explicitSourceId.isEmpty()) {
    return explicitSourceId == m_sourceName;
  }

  // Fallback to request payload
  QJsonObject req = sourceModel()->data(index, ErrorsModel::RequestRole).toJsonObject();
  QString source = req.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString();
  if (source.isEmpty()) {
    source = req.value(QStringLiteral("source")).toString();
  }
'''

content = re.sub(
    r'bool ErrorFilterProxyModel::filterAcceptsRow.*?if \(source.isEmpty\(\)\) \{\n    source = req.value\(QStringLiteral\("source"\)\)\.toString\(\);\n  \}',
    impl,
    content,
    flags=re.DOTALL
)

with open('src/sourcestatuswidget.cpp', 'w') as f:
    f.write(content)
