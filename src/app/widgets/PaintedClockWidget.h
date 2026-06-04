#ifndef ETEST_APP_PAINTED_CLOCK_WIDGET_H_
#define ETEST_APP_PAINTED_CLOCK_WIDGET_H_

#include <QWidget>

class PaintedClockWidget : public QWidget {
  Q_OBJECT
 public:
  explicit PaintedClockWidget(QWidget* parent = nullptr);

 protected:
  void paintEvent(QPaintEvent* event) override;
};

#endif  // ETEST_APP_PAINTED_CLOCK_WIDGET_H_
