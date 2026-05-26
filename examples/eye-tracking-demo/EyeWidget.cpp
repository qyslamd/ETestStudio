#include "EyeWidget.h"

#include <QPainter>
#include <QEnterEvent>
#include <QtMath>

EyeWidget::EyeWidget(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  mouse_pos_ = QPointF(0, 0);
}

void EyeWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // background
  painter.fillRect(rect(), QColor(0x1e, 0x1e, 0x2e));

  auto fg = rect();
  double cx = fg.width() / 2.0;
  double cy = fg.height() / 2.0;
  double eyeRadius = qMin(fg.width(), fg.height()) / 5.0;
  double eyeSpacing = eyeRadius * 1.6;

  QPointF leftCenter(cx - eyeSpacing, cy);
  QPointF rightCenter(cx + eyeSpacing, cy);

  auto offset = mouse_inside_ ? clampedPupilOffset(leftCenter, eyeRadius * 0.55)
                              : QPointF(0, 0);

  drawEye(painter, leftCenter, eyeRadius, offset);
  drawEye(painter, rightCenter, eyeRadius, offset);
}

void EyeWidget::drawEye(QPainter& painter, const QPointF& center,
                        double radius, const QPointF& pupilOffset) {
  // eye white
  painter.setBrush(QColor(0xff, 0xff, 0xff));
  painter.setPen(QPen(QColor(0xcc, 0xcc, 0xcc), 2));
  painter.drawEllipse(center, radius, radius);

  // pupil
  double pupilR = radius * 0.35;
  QPointF pupilPos = center + pupilOffset;
  painter.setBrush(QColor(0x2c, 0x2c, 0x2c));
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(pupilPos, pupilR, pupilR);
}

QPointF EyeWidget::clampedPupilOffset(const QPointF& eyeCenter,
                                      double maxRadius) const {
  QPointF vec(mouse_pos_.x() - eyeCenter.x(),
              mouse_pos_.y() - eyeCenter.y());
  double dist = qSqrt(vec.x() * vec.x() + vec.y() * vec.y());
  if (dist <= maxRadius)
    return vec;
  return vec * (maxRadius / dist);
}

void EyeWidget::mouseMoveEvent(QMouseEvent* event) {
  mouse_pos_ = event->pos();
  update();
}

bool EyeWidget::event(QEvent* event) {
  if (event->type() == QEvent::Enter) {
    mouse_inside_ = true;
    update();
  } else if (event->type() == QEvent::Leave) {
    mouse_inside_ = false;
    mouse_pos_ = QPointF(0, 0);
    update();
  }
  return QWidget::event(event);
}
