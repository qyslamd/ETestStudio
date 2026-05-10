#include "eph_qt_switch_button.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace etest::app {

namespace {

void calcStartEndPoint(const QRectF& rect, qreal angle, QPointF& start,
                       QPointF& end) {
  decltype(angle) angle1 = abs(angle);
  if (angle1 > 360) {
    int integerAngle = static_cast<int>(angle1);
    qreal floatRemain = angle1 - integerAngle;
    auto remain = integerAngle % 360;
    angle1 = remain + floatRemain;
  }

  auto halfW = rect.width() / 2.0;
  auto halfH = rect.height() / 2.0;
  auto center = rect.center();
  auto quarterAngle = qRadiansToDegrees(atan2(halfH, halfW));
  auto radian = qDegreesToRadians(angle1);

  qreal dxStart = 0, dyStart = 0, dxEnd = 0, dyEnd = 0;
  if (angle1 >= 0 && angle1 < quarterAngle) {
    auto dy = tan(radian) * halfW;
    dxStart = halfW; dyStart = -dy;
    dxEnd = -halfW; dyEnd = dy;
  } else if (angle1 >= quarterAngle && angle1 < 90) {
    auto dx = halfW / tan(radian);
    dxStart = dx; dyStart = -halfH;
    dxEnd = -dx; dyEnd = halfH;
  } else if (angle1 >= 90 && angle1 < 90 + quarterAngle) {
    auto dx = tan(qDegreesToRadians(angle1 - 90)) * halfH;
    dxStart = -dx; dyStart = -halfH;
    dxEnd = dx; dyEnd = halfH;
  } else if (angle1 >= 90 + quarterAngle && angle1 < 180) {
    auto dy = tan(qDegreesToRadians(180.0 - angle1)) * halfW;
    dxStart = -halfW; dyStart = -dy;
    dxEnd = halfW; dyEnd = dy;
  } else if (angle1 >= 180 && angle1 < 180 + quarterAngle) {
    auto dy = tan(radian) * halfW;
    dxStart = -halfW; dyStart = dy;
    dxEnd = halfW; dyEnd = -dy;
  } else if (angle1 >= 180 + quarterAngle && angle1 < 270) {
    auto dx = halfW / tan(radian);
    dxStart = -dx; dyStart = halfH;
    dxEnd = dx; dyEnd = -halfH;
  } else if (angle1 >= 270 && angle1 < 270 + quarterAngle) {
    auto dx = tan(qDegreesToRadians(angle1 - 270)) * halfH;
    dxStart = dx; dyStart = halfH;
    dxEnd = -dx; dyEnd = -halfH;
  } else if (angle1 >= 270 + quarterAngle && angle1 < 360) {
    auto dy = tan(qDegreesToRadians(360 - angle1)) * halfW;
    dxStart = halfW; dyStart = dy;
    dxEnd = -halfW; dyEnd = -dy;
  }

  start = center;
  end = center;
  if (angle >= 0) {
    start.rx() += dxStart; start.ry() += dyStart;
    end.rx() += dxEnd; end.ry() += dyEnd;
  } else {
    start.rx() += dxEnd; start.ry() += dyEnd;
    end.rx() += dxStart; end.ry() += dyStart;
  }
}

}  // namespace

EphQtSwitchButton::EphQtSwitchButton(QWidget* parent)
    : QAbstractButton(parent) {
  setCursor(Qt::PointingHandCursor);
  setCheckable(true);
}

void EphQtSwitchButton::setOnBackground(const QBrush& brush) {
  background_on_ = brush;
  update();
}

void EphQtSwitchButton::setOffBackground(const QBrush& brush) {
  background_off_ = brush;
  update();
}

void EphQtSwitchButton::setCheckedText(const QString& text) {
  checked_text_ = text;
}

void EphQtSwitchButton::setUnCheckedText(const QString& text) {
  unchecked_text_ = text;
}

void EphQtSwitchButton::paintEvent(QPaintEvent* event) {
  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing);
  paintButton(&p);
  paintText(&p);
}

QSize EphQtSwitchButton::sizeHint() const {
  return QSize(96, 42);
}

QSize EphQtSwitchButton::minimumSizeHint() const {
  return QSize(72, 30);
}

void EphQtSwitchButton::paintButton(QPainter* p) {
  // level-0: whole background
  {
    QRectF rect = this->rect();
    QLinearGradient linearGrad;
    QPointF start, end;
    calcStartEndPoint(rect, 135, start, end);
    linearGrad.setStart(start);
    linearGrad.setFinalStop(end);
    linearGrad.setColorAt(0, QColor("#c4c5c7"));
    linearGrad.setColorAt(0.52, QColor("#dcdddf"));
    linearGrad.setColorAt(1, QColor("#ebebeb"));
    fillRoundRect(p, rect, linearGrad);
  }

  // level-1: on/off background
  {
    qreal margin = std::min(this->height(), this->width()) / 10.0;
    QRectF rect = this->rect().marginsRemoved(
        QMargins(margin, margin, margin, margin));
    QLinearGradient linearGrad;
    QPointF start, end;
    calcStartEndPoint(rect, -225, start, end);
    linearGrad.setStart(start);
    linearGrad.setFinalStop(end);
    if (isChecked()) {
      linearGrad.setColorAt(0, QColor("#f9f047"));
      linearGrad.setColorAt(1, QColor("#0fd850"));
    } else {
      linearGrad.setColorAt(0, QColor("#ff0844"));
      linearGrad.setColorAt(1, QColor("#ffb199"));
    }
    fillRoundRect(p, rect, linearGrad);
  }

  // level-2: on/off slider outer
  {
    qreal margin = std::min(this->height(), this->width()) / 10.0;
    QRectF rect = this->rect().marginsRemoved(
        QMargins(margin, margin, margin, margin));
    if (isChecked()) {
      rect.setX(rect.width() / 2.0);
    } else {
      rect.setWidth(rect.width() / 2.0);
    }
    QLinearGradient linearGrad;
    QPointF start, end;
    calcStartEndPoint(rect, 90, start, end);
    linearGrad.setStart(start);
    linearGrad.setFinalStop(end);
    linearGrad.setColorAt(1, QColor("#c4c5c7"));
    linearGrad.setColorAt(0.52, QColor("#dcdddf"));
    linearGrad.setColorAt(0, QColor("#ebebeb"));
    fillRoundRect(p, rect, linearGrad);
  }

  // level-3: on/off slider inner with lines
  {
    qreal margin = std::min(this->height(), this->width()) / 10.0;
    QRectF rect = this->rect().marginsRemoved(
        QMargins(margin, margin, margin, margin));
    rect = rect.marginsRemoved(QMargins(margin, margin, margin, margin));
    if (isChecked()) {
      rect.setX(rect.width() / 2.0 + 2.0 * margin);
    } else {
      rect.setWidth(rect.width() / 2.0 - margin);
    }
    QLinearGradient linearGrad;
    QPointF start, end;
    calcStartEndPoint(rect, 90, start, end);
    linearGrad.setStart(start);
    linearGrad.setFinalStop(end);
    linearGrad.setColorAt(0, QColor("#c4c5c7"));
    linearGrad.setColorAt(0.52, QColor("#dcdddf"));
    linearGrad.setColorAt(1, QColor("#ebebeb"));
    fillRoundRect(p, rect, linearGrad);

    auto x0 = rect.x();
    auto y0 = rect.y();
    auto delta = rect.height() / 5.0;
    QPointF p1(x0 + 1.0 * rect.width() / 4.0, y0 + delta);
    QPointF p2(x0 + 1.0 * rect.width() / 4.0, y0 + rect.height() - delta);
    QPointF p11(x0 + 2.0 * rect.width() / 4.0, y0 + delta);
    QPointF p22(x0 + 2.0 * rect.width() / 4.0, y0 + rect.height() - delta);
    QPointF p111(x0 + 3.0 * rect.width() / 4.0, y0 + delta);
    QPointF p222(x0 + 3.0 * rect.width() / 4.0, y0 + rect.height() - delta);

    p->save();
    p->setPen(QPen(Qt::gray, delta / 2.0, Qt::SolidLine, Qt::RoundCap,
                   Qt::RoundJoin));
    p->drawLine(p1, p2);
    p->drawLine(p11, p22);
    p->drawLine(p111, p222);
    p->restore();
  }
}

void EphQtSwitchButton::fillRoundRect(QPainter* p, const QRectF& rect,
                                      const QBrush& brush) {
  p->save();
  QPainterPath path;
  auto radius = rect.height() / 2.0;
  path.addRoundedRect(rect, radius, radius);
  p->fillPath(path, brush);
  p->restore();
}

void EphQtSwitchButton::paintText(QPainter* p) {
  p->save();
  qreal margin = std::min(this->height(), this->width()) / 10.0;
  QRectF rect = this->rect().marginsRemoved(
      QMargins(margin, margin, margin, margin));
  rect = rect.marginsRemoved(QMargins(margin, margin, margin, margin));
  if (isChecked()) {
    rect.setWidth(rect.width() / 2.0 - margin);
  } else {
    rect.setX(rect.width() / 2.0 + 2.0 * margin);
  }

  QFont font("Microsoft YaHei");
  font.setPixelSize(12);
  font.setBold(true);
  p->setFont(font);
  auto pen = p->pen();
  pen.setColor(Qt::white);
  p->setPen(pen);
  p->drawText(rect, Qt::AlignCenter,
              isChecked() ? checked_text_ : unchecked_text_);
  p->restore();
}

}  // namespace etest::app
