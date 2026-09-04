#include "SwitchButton.h"

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace etest::ui {

SwitchButton::SwitchButton(QWidget* parent) : QAbstractButton(parent) {
  setCursor(Qt::PointingHandCursor);
  setCheckable(true);

  knob_animation_ = new QVariantAnimation(this);
  knob_animation_->setDuration(180);
  knob_animation_->setEasingCurve(QEasingCurve::InOutCubic);
  connect(knob_animation_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            knob_position_ = value.toReal();
            update();
          });
  connect(this, &QAbstractButton::toggled, this,
          &SwitchButton::startKnobAnimation);
}

void SwitchButton::setOnBackground(const QBrush& brush) {
  background_on_ = brush;
  update();
}

void SwitchButton::setOffBackground(const QBrush& brush) {
  background_off_ = brush;
  update();
}

void SwitchButton::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing);

  // 胶囊底：开/关背景色
  fillRoundRect(&p, rect(), isChecked() ? background_on_ : background_off_);

  // 白色圆形滑块（附轻微投影）
  // 几何做钳制：控件退化尺寸（高过小/宽小于高）时不产生负直径或负行程
  const qreal d = qMax<qreal>(4.0, height() - 4.0);  // 直径
  const qreal left = 2.0;
  const qreal right = qMax(left, width() - d - 2.0);
  const qreal x = left + (right - left) * knob_position_;
  const qreal y = (height() - d) / 2.0;  // 垂直居中

  fillRoundRect(&p, QRectF(x, y + 1.0, d, d), QColor(0, 0, 0, 46));
  fillRoundRect(&p, QRectF(x, y, d, d), Qt::white);
}

QSize SwitchButton::sizeHint() const {
  return QSize(44, 24);
}

QSize SwitchButton::minimumSizeHint() const {
  return QSize(44, 24);
}

void SwitchButton::fillRoundRect(QPainter* p,
                                 const QRectF& rect,
                                 const QBrush& brush) {
  p->save();
  QPainterPath path;
  path.addRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);
  p->fillPath(path, brush);
  p->restore();
}

void SwitchButton::startKnobAnimation(bool checked) {
  knob_animation_->stop();
  knob_animation_->setStartValue(knob_position_);
  knob_animation_->setEndValue(checked ? 1.0 : 0.0);
  knob_animation_->start();
}

}  // namespace etest::ui
