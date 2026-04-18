with open("src/mainwindow.cpp", "r") as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    new_lines.append(line)
    if 'QAction *rawTranscriptAction = menu.addAction(i18n("Raw Transcript"));' in line:
        new_lines.append('          QAction *requeueAction = menu.addAction(i18n("Requeue"));\n')
    if 'connect(copyTemplateAction, &QAction::triggered, [this, index]() {' in line:
        new_lines.insert(len(new_lines)-1, '          connect(requeueAction, &QAction::triggered, [this]() {\n')
        new_lines.insert(len(new_lines)-1, '            QModelIndexList selectedRows = m_errorsView->selectionModel()->selectedRows();\n')
        new_lines.insert(len(new_lines)-1, '            QList<int> rowsToRequeue;\n')
        new_lines.insert(len(new_lines)-1, '            for (const QModelIndex &idx : selectedRows) {\n')
        new_lines.insert(len(new_lines)-1, '              if (!rowsToRequeue.contains(idx.row())) {\n')
        new_lines.insert(len(new_lines)-1, '                rowsToRequeue.append(idx.row());\n')
        new_lines.insert(len(new_lines)-1, '              }\n')
        new_lines.insert(len(new_lines)-1, '            }\n')
        new_lines.insert(len(new_lines)-1, '            std::sort(rowsToRequeue.begin(), rowsToRequeue.end(), std::greater<int>());\n')
        new_lines.insert(len(new_lines)-1, '            for (int row : rowsToRequeue) {\n')
        new_lines.insert(len(new_lines)-1, '              QJsonObject errData = m_errorsModel->getError(row);\n')
        new_lines.insert(len(new_lines)-1, '              QJsonObject req = errData.value(QStringLiteral("request")).toObject();\n')
        new_lines.insert(len(new_lines)-1, '              QueueItem item;\n')
        new_lines.insert(len(new_lines)-1, '              item.requestData = req;\n')
        new_lines.insert(len(new_lines)-1, '              if (errData.contains(QStringLiteral("pastErrors"))) {\n')
        new_lines.insert(len(new_lines)-1, '                item.pastErrors = errData.value(QStringLiteral("pastErrors")).toArray();\n')
        new_lines.insert(len(new_lines)-1, '              }\n')
        new_lines.insert(len(new_lines)-1, '              QJsonObject strippedError = errData;\n')
        new_lines.insert(len(new_lines)-1, '              strippedError.remove(QStringLiteral("pastErrors"));\n')
        new_lines.insert(len(new_lines)-1, '              item.pastErrors.append(strippedError);\n')
        new_lines.insert(len(new_lines)-1, '              m_queueModel->enqueueItem(item);\n')
        new_lines.insert(len(new_lines)-1, '              m_errorsModel->removeError(row);\n')
        new_lines.insert(len(new_lines)-1, '            }\n')
        new_lines.insert(len(new_lines)-1, '            updateStatus(i18n("Requeued selected errors."));\n')
        new_lines.insert(len(new_lines)-1, '          });\n')
        new_lines.insert(len(new_lines)-1, '\n')

with open("src/mainwindow.cpp", "w") as f:
    f.writelines(new_lines)
