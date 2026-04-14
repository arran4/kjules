#include "clickableprogressbar.h"

ClickableProgressBar::ClickableProgressBar(QWidget *parent) : QProgressBar(parent) {
}

ClickableProgressBar::~ClickableProgressBar() {}

void ClickableProgressBar::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    Q_EMIT clicked();
  }
  QProgressBar::mouseReleaseEvent(event);
}
