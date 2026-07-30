#include "src/filterparser.h"
#include <QDebug>
#include <QStringList>

class MockAccessor : public FilterDataAccessor {
public:
  QString getValue(const QString &key) const override {
    if (key == "private") return "Yes";
    return "";
  }
  QList<QString> getAllValues() const override { return {}; }
};

int main() {
  QSharedPointer<ASTNode> ast = FilterParser::parse("private:true");
  MockAccessor acc;
  qDebug() << "private:true ->" << ast->evaluate(acc);

  ast = FilterParser::parse("private:Yes");
  qDebug() << "private:Yes ->" << ast->evaluate(acc);
  return 0;
}
