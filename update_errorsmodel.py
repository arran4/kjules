import re

with open('src/errorsmodel.h', 'r') as f:
    content = f.read()

# Add roles to ErrorRoles enum
content = re.sub(
    r'enum ErrorRoles { RequestRole = Qt::UserRole \+ 1, ResponseRole, MessageRole, HttpDetailsRole, TimestampRole };',
    'enum ErrorRoles { RequestRole = Qt::UserRole + 1, ResponseRole, MessageRole, HttpDetailsRole, TimestampRole,\n                    SeenRole, UnseenRole, SourceIdRole, SessionIdRole, OperationRole, ProviderRole };',
    content
)

# Add unseenCount, markSeen, markAllSeen methods
content = re.sub(
    r'void clear\(\);',
    'void clear();\n\n  int unseenCount() const;\n  void markSeen(int row);\n  void markAllSeen();\n\nQ_SIGNALS:\n  void unseenCountChanged(int count);',
    content
)

# Add unseen array
content = re.sub(
    r'QJsonArray m_errors;',
    'QJsonArray m_errors;\n  QList<bool> m_seenState;\n  int m_unseenCount = 0;\n  void updateUnseenCount();',
    content
)

with open('src/errorsmodel.h', 'w') as f:
    f.write(content)
