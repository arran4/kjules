with open('src/sessionmodel.cpp', 'r') as f:
    content = f.read()

content = content.replace('void SessionModel::setSessions(const QJsonArray &sessions) {\n  beginResetModel();', 'void SessionModel::setSessions(const QJsonArray &sessions) {\n  beginResetModel();')

# Instead of relying on a heuristic, let's just emit it explicitly at the end of saveSessions()
# Actually, saveSessions() is called whenever data changes. So emitting it there is perfect.
