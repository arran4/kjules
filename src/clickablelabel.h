#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QMouseEvent>

class ClickableLabel : public QLabel {
  Q_OBJECT

public:
  explicit ClickableLabel(QWidget *parent = nullptr) : QLabel(parent) {
    setCursor(Qt::PointingHandCursor);
    connect(this, &QLabel::linkActivated, this, [this]() { m_linkJustActivated = true; });
  }
  explicit ClickableLabel(const QString &text, QWidget *parent = nullptr) : QLabel(text, parent) {
    setCursor(Qt::PointingHandCursor);
    connect(this, &QLabel::linkActivated, this, [this]() { m_linkJustActivated = true; });
  }

Q_SIGNALS:
  void clicked();

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      QLabel::mousePressEvent(event);
      if (!event->isAccepted()) {
        event->accept();
      }
    } else {
      QLabel::mousePressEvent(event);
    }
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      m_linkJustActivated = false;
      QLabel::mouseReleaseEvent(event);
      if (!m_linkJustActivated) {
        Q_EMIT clicked();
      }
      event->accept();
    } else {
      QLabel::mouseReleaseEvent(event);
    }
  }

private:
  bool m_linkJustActivated = false;
};

#endif // CLICKABLELABEL_H
