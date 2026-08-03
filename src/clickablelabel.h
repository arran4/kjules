#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QMouseEvent>

class ClickableLabel : public QLabel {
  Q_OBJECT

public:
  explicit ClickableLabel(QWidget *parent = nullptr) : QLabel(parent) { setCursor(Qt::PointingHandCursor); }
  explicit ClickableLabel(const QString &text, QWidget *parent = nullptr) : QLabel(text, parent) {
    setCursor(Qt::PointingHandCursor);
  }

Q_SIGNALS:
  void clicked();

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      event->accept();
    } else {
      QLabel::mousePressEvent(event);
    }
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      Q_EMIT clicked();
      event->accept();
    } else {
      QLabel::mouseReleaseEvent(event);
    }
  }
};

#endif // CLICKABLELABEL_H
