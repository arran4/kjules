import re

with open('src/errorsmodel.cpp', 'r') as f:
    content = f.read()

# Add to roleNames
content = re.sub(
    r'roles\[TimestampRole\] = "timestamp";',
    'roles[TimestampRole] = "timestamp";\n  roles[SeenRole] = "seen";\n  roles[UnseenRole] = "unseen";\n  roles[SourceIdRole] = "sourceId";\n  roles[SessionIdRole] = "sessionId";\n  roles[OperationRole] = "operation";\n  roles[ProviderRole] = "provider";',
    content
)

# Add to data switch statement
data_cases = '''  case SeenRole:
    return m_seenState.at(index.row());
  case UnseenRole:
    return !m_seenState.at(index.row());
  case SourceIdRole:
    return error.value(QStringLiteral("sourceId")).toString();
  case SessionIdRole:
    return error.value(QStringLiteral("sessionId")).toString();
  case OperationRole:
    return error.value(QStringLiteral("operation")).toString();
  case ProviderRole:
    return error.value(QStringLiteral("provider")).toString();
'''
content = re.sub(
    r'case TimestampRole:\n    return error\.value\(QStringLiteral\("timestamp"\)\)\.toString\(\);\n  default:\n    return QVariant\(\);\n  \}',
    'case TimestampRole:\n    return error.value(QStringLiteral("timestamp")).toString();\n' + data_cases + '  default:\n    return QVariant();\n  }',
    content
)

# loadErrors - ensure trim to 200 and start seen
load_impl = '''void ErrorsModel::loadErrors() {
  QString filePath = cacheFilePath();
  QFile file(filePath);
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_errors = doc.array();
    file.close();

    // Trim to 200 on load
    while (m_errors.size() > 200) {
      m_errors.removeLast();
    }
  }

  m_seenState.clear();
  for (int i = 0; i < m_errors.size(); ++i) {
    m_seenState.append(true); // Loaded errors start as seen
  }
  updateUnseenCount();
}'''

content = re.sub(
    r'void ErrorsModel::loadErrors\(\) \{.*?\n\}',
    load_impl,
    content,
    flags=re.DOTALL
)

# addErrorObj - insert unseen, trim to 200
add_impl = '''void ErrorsModel::addErrorObj(const QJsonObject &errorObj) {
  beginInsertRows(QModelIndex(), 0, 0);
  m_errors.insert(0, errorObj);
  m_seenState.insert(0, false); // New errors start as unseen
  endInsertRows();

  while (m_errors.size() > 200) {
    beginRemoveRows(QModelIndex(), m_errors.size() - 1, m_errors.size() - 1);
    m_errors.removeLast();
    m_seenState.removeLast();
    endRemoveRows();
  }

  updateUnseenCount();
  saveErrors();
}'''
content = re.sub(
    r'void ErrorsModel::addErrorObj\(const QJsonObject &errorObj\) \{.*?\n\}',
    add_impl,
    content,
    flags=re.DOTALL
)

# removeError - remove seen state
remove_impl = '''void ErrorsModel::removeError(int row) {
  if (row >= 0 && row < m_errors.size()) {
    beginRemoveRows(QModelIndex(), row, row);
    m_errors.removeAt(row);
    m_seenState.removeAt(row);
    endRemoveRows();
    updateUnseenCount();
    saveErrors();
  }
}'''
content = re.sub(
    r'void ErrorsModel::removeError\(int row\) \{.*?\n\}',
    remove_impl,
    content,
    flags=re.DOTALL
)

# clear
clear_impl = '''void ErrorsModel::clear() {
  beginResetModel();
  m_errors = QJsonArray();
  m_seenState.clear();
  endResetModel();
  updateUnseenCount();
  saveErrors();
}'''
content = re.sub(
    r'void ErrorsModel::clear\(\) \{.*?\n\}',
    clear_impl,
    content,
    flags=re.DOTALL
)

# new methods
new_methods = '''
int ErrorsModel::unseenCount() const {
  return m_unseenCount;
}

void ErrorsModel::updateUnseenCount() {
  int count = 0;
  for (bool seen : m_seenState) {
    if (!seen) count++;
  }
  if (m_unseenCount != count) {
    m_unseenCount = count;
    Q_EMIT unseenCountChanged(m_unseenCount);
  }
}

void ErrorsModel::markSeen(int row) {
  if (row >= 0 && row < m_seenState.size() && !m_seenState[row]) {
    m_seenState[row] = true;
    Q_EMIT dataChanged(index(row, 0), index(row, 0), {SeenRole, UnseenRole});
    updateUnseenCount();
  }
}

void ErrorsModel::markAllSeen() {
  bool changed = false;
  for (int i = 0; i < m_seenState.size(); ++i) {
    if (!m_seenState[i]) {
      m_seenState[i] = true;
      changed = true;
    }
  }
  if (changed) {
    Q_EMIT dataChanged(index(0, 0), index(m_seenState.size() - 1, 0), {SeenRole, UnseenRole});
    updateUnseenCount();
  }
}
'''
content += new_methods

with open('src/errorsmodel.cpp', 'w') as f:
    f.write(content)
