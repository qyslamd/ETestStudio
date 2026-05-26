#ifndef ETEST_EXAMPLES_EYETRACKING_EYEWIDGET_H_
#define ETEST_EXAMPLES_EYETRACKING_EYEWIDGET_H_

#include <QPoint>
#include <QWidget>

class EyeWidget : public QWidget {
  Q_OBJECT

 public:
  explicit EyeWidget(QWidget* parent = nullptr);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  bool event(QEvent* event) override;

 private:
  void drawEye(QPainter& painter, const QPointF& center, double radius,
               const QPointF& pupilOffset);
  QPointF clampedPupilOffset(const QPointF& eyeCenter, double maxRadius) const;

  QPointF mouse_pos_;
  bool mouse_inside_ = false;
};

#endif  // ETEST_EXAMPLES_EYETRACKING_EYEWIDGET_H_
