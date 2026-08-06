import re

with open('src/newsessiondialog.cpp', 'r') as f:
    content = f.read()

old_abstract = """  class ProxyFilterDataAccessor : public FilterDataAccessor {
  public:
    ProxyFilterDataAccessor(const QModelIndex &index, const SourceModel *model) : m_index(index), m_model(model) {}

    QString getValue(const QString &key) const override {"""

new_abstract = """  class ProxyFilterDataAccessor : public FilterDataAccessor {
  public:
    ProxyFilterDataAccessor(const QModelIndex &index, const SourceModel *model) : m_index(index), m_model(model) {}
    QList<QString> getAllValues() const override { return {}; }

    QString getValue(const QString &key) const override {"""

content = content.replace(old_abstract, new_abstract)

# and fix setInitialData which has another getDefaultBranch call
old_set_initial = """        if (branch.isEmpty()) {
          m_selectedSources.insert(name, getDefaultBranch(matches.first()));
        } else {"""
new_set_initial = """        if (branch.isEmpty()) {
          QStringList defaults = getDefaultBranches(matches.first());
          m_selectedSources.insert(name, defaults.isEmpty() ? QStringLiteral("main") : defaults.first());
        } else {"""

content = content.replace(old_set_initial, new_set_initial)

with open('src/newsessiondialog.cpp', 'w') as f:
    f.write(content)
