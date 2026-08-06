import re
with open('src/newsessiondialog.cpp', 'r') as f:
    content = f.read()

old_code = """      } else {
        QString name = val.toString();
        QModelIndexList matches =
            m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::NameRole, name, 1, Qt::MatchExactly);
        if (!matches.isEmpty()) {
          m_selectedSources.insert(name, getDefaultBranch(matches.first()));
        } else {
          m_selectedSources.insert(name, QStringLiteral("main"));
        }
      }"""

new_code = """      } else {
        QString name = val.toString();
        QModelIndexList matches =
            m_sourceModel->match(m_sourceModel->index(0, 0), SourceModel::NameRole, name, 1, Qt::MatchExactly);
        if (!matches.isEmpty()) {
          QStringList defaults = getDefaultBranches(matches.first());
          m_selectedSources.insert(name, defaults.isEmpty() ? QStringLiteral("main") : defaults.first());
        } else {
          m_selectedSources.insert(name, QStringLiteral("main"));
        }
      }"""

content = content.replace(old_code, new_code)

with open('src/newsessiondialog.cpp', 'w') as f:
    f.write(content)
