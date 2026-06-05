#ifndef ETEST_APP_PAINTED_CLOCK_WIDGET_H_
#define ETEST_APP_PAINTED_CLOCK_WIDGET_H_

#include <QWidget>

class IClockRenderer;

class PaintedClockWidget : public QWidget {
  Q_OBJECT
 public:
  explicit PaintedClockWidget(QWidget* parent = nullptr);
  ~PaintedClockWidget() override;

  void setRenderer(IClockRenderer* renderer);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  IClockRenderer* renderer_ = nullptr;
};

#endif  // ETEST_APP_PAINTED_CLOCK_WIDGET_H_
