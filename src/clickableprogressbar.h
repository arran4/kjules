#ifndef CLICKABLEPROGRESSBAR_H
#define CLICKABLEPROGRESSBAR_H

#include <QMouseEvent>
#include <QProgressBar>

class ClickableProgressBar : public QProgressBar {
  Q_OBJECT

public:
  explicit ClickableProgressBar(QWidget *parent = nullptr);
  ~ClickableProgressBar() override;

Q_SIGNALS:
  void clicked();

protected:
  void mouseReleaseEvent(QMouseEvent *event) override;
};

#endif // CLICKABLEPROGRESSBAR_H
