#include <QSortFilterProxyModel>
class Test : public QSortFilterProxyModel {
public:
  void test() {
    invalidate();
  }
};
