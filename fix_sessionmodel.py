import re

with open('src/sessionmodel.cpp', 'r') as f:
    lines = f.readlines()

out = []
for line in lines:
    out.append(line)
    if line.strip() == "saveSessions();":
        if "void SessionModel::setSessions" in "".join(out[-20:]):
            out.append("  Q_EMIT sessionsLoadedOrUpdated();\n")
        elif "void SessionModel::addSessions" in "".join(out[-20:]):
            out.append("  Q_EMIT sessionsLoadedOrUpdated();\n")
        elif "void SessionModel::addSession" in "".join(out[-20:]):
            out.append("  Q_EMIT sessionsLoadedOrUpdated();\n")
        elif "void SessionModel::updateSession" in "".join(out[-20:]):
            out.append("  Q_EMIT sessionsLoadedOrUpdated();\n")
        elif "void SessionModel::removeSession" in "".join(out[-20:]):
            out.append("  Q_EMIT sessionsLoadedOrUpdated();\n")

with open('src/sessionmodel.cpp', 'w') as f:
    f.writelines(out)
