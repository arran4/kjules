import re
with open('src/newsessiondialog.cpp', 'r') as f:
    content = f.read()

old_abstract = """    QList<QString> getAllValues() const override { return {}; }"""
new_abstract = """    QList<QString> getAllValues() const override {
      QList<QString> values;
      values.append(m_model->data(m_index, SourceModel::NameRole).toString());
      values.append(m_model->data(m_index.siblingAtColumn(0), Qt::DisplayRole).toString());
      return values;
    }"""

content = content.replace(old_abstract, new_abstract)
with open('src/newsessiondialog.cpp', 'w') as f:
    f.write(content)
