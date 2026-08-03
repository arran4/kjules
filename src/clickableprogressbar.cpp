#include "clickableprogressbar.h"

ClickableProgressBar::ClickableProgressBar(QWidget *parent) : QProgressBar(parent) {}

ClickableProgressBar::~ClickableProgressBar() {}

void ClickableProgressBar::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    event->accept();
  } else {
    QProgressBar::mousePressEvent(event);
  }
}

void ClickableProgressBar::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    Q_EMIT clicked();
    event->accept();
  } else {
    QProgressBar::mouseReleaseEvent(event);
  }
}
