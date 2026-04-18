with open("src/mainwindow.cpp", "r") as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    new_lines.append(line)
    if 'updateStatus(i18n("Sending error item immediately..."));' in line:
        new_lines.append('              });\n')
        new_lines.append('\n')
        new_lines.append('              connect(window, &ErrorWindow::requeueRequested, [this](int row) {\n')
        new_lines.append('                QJsonObject errData = m_errorsModel->getError(row);\n')
        new_lines.append('                QJsonObject req = errData.value(QStringLiteral("request")).toObject();\n')
        new_lines.append('                QueueItem item;\n')
        new_lines.append('                item.requestData = req;\n')
        new_lines.append('                if (errData.contains(QStringLiteral("pastErrors"))) {\n')
        new_lines.append('                  item.pastErrors = errData.value(QStringLiteral("pastErrors")).toArray();\n')
        new_lines.append('                }\n')
        new_lines.append('                QJsonObject strippedError = errData;\n')
        new_lines.append('                strippedError.remove(QStringLiteral("pastErrors"));\n')
        new_lines.append('                item.pastErrors.append(strippedError);\n')
        new_lines.append('                m_queueModel->enqueueItem(item);\n')
        new_lines.append('                m_errorsModel->removeError(row);\n')
        new_lines.append('                updateStatus(i18n("Error requeued."));\n')

with open("src/mainwindow.cpp", "w") as f:
    f.writelines(new_lines)
