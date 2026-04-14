#ifndef CLICKABLEPROGRESSBAR_H
#define CLICKABLEPROGRESSBAR_H

#include <QProgressBar>
#include <QMouseEvent>

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
