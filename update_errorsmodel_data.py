import re

with open('src/errorsmodel.cpp', 'r') as f:
    content = f.read()

# Add to data switch statement safely
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
    r'(case TimestampRole:.*?return QVariant\(\);\n  \})',
    r'\1',
    content,
    flags=re.DOTALL
)

# Wait, let's just find `default:\n    return QVariant();` and replace it
content = re.sub(
    r'  default:\n    return QVariant\(\);',
    data_cases + '  default:\n    return QVariant();',
    content
)

# Also check for double-adds
content = re.sub(
    r'case SeenRole:[\s\S]*?case SeenRole:',
    'case SeenRole:',
    content
)

with open('src/errorsmodel.cpp', 'w') as f:
    f.write(content)
