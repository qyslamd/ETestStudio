#include "PenguinOverlay.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QtMath>

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

PenguinOverlay::PenguinOverlay(bool fromLeft)
    : QWidget(nullptr), fromLeft_(fromLeft) {
  setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setMouseTracking(true);

  resize(100, 120);

  // Position at the starting edge, on the primary screen
  QRect screen = QGuiApplication::primaryScreen()->geometry();
  floorY_ = screen.height() - 140;

  int x = fromLeft_ ? -width() : screen.width();
  move(x, static_cast<int>(floorY_));

  // Walk animation – traverses screen width in ~8 s
  walkAnim_ = new QPropertyAnimation(this, "walkPos", this);
  walkAnim_->setDuration(8000);
  walkAnim_->setStartValue(0.0);
  walkAnim_->setEndValue(static_cast<qreal>(screen.width() + width()));
  walkAnim_->setEasingCurve(QEasingCurve::Linear);

  // Body bob – subtle up/down while walking
  bobAnim_ = new QPropertyAnimation(this, "bodyBob", this);
  bobAnim_->setDuration(400);
  bobAnim_->setStartValue(0.0);
  bobAnim_->setEndValue(4.0);
  bobAnim_->setEasingCurve(QEasingCurve::InOutSine);

  // Wing flap
  wingAnim_ = new QPropertyAnimation(this, "wingAngle", this);
  wingAnim_->setDuration(300);
  wingAnim_->setStartValue(-15.0);
  wingAnim_->setEndValue(15.0);
  wingAnim_->setEasingCurve(QEasingCurve::InOutSine);

  connect(walkAnim_, &QPropertyAnimation::finished, this,
          &PenguinOverlay::onWalkFinished);

  // Loop bob & wing while walking
  connect(bobAnim_, &QPropertyAnimation::finished, this, [this]() {
    if (!fleeing_) {
      bobAnim_->setDirection(bobAnim_->direction() == QAbstractAnimation::Forward
                                 ? QAbstractAnimation::Backward
                                 : QAbstractAnimation::Forward);
      bobAnim_->start();
    }
  });
  connect(wingAnim_, &QPropertyAnimation::finished, this, [this]() {
    if (!fleeing_) {
      wingAnim_->setDirection(
          wingAnim_->direction() == QAbstractAnimation::Forward
              ? QAbstractAnimation::Backward
              : QAbstractAnimation::Forward);
      wingAnim_->start();
    }
  });

  startWalk();
}

// ---------------------------------------------------------------------------
//  Property setters (drive repaint)
// ---------------------------------------------------------------------------

void PenguinOverlay::setWalkPos(qreal v) {
  walkPos_ = v;
  QRect screen = QGuiApplication::primaryScreen()->geometry();
  int x = fromLeft_
              ? static_cast<int>(screen.left() - width() + walkPos_)
              : static_cast<int>(screen.right() - walkPos_);
  move(x, static_cast<int>(floorY_ + bodyBob_));
}

void PenguinOverlay::setBodyBob(qreal v) {
  bodyBob_ = v;
  QRect screen = QGuiApplication::primaryScreen()->geometry();
  move(x(), static_cast<int>(floorY_ + v));
}

void PenguinOverlay::setWingAngle(qreal v) {
  wingAngle_ = v;
  update();
}

// ---------------------------------------------------------------------------
//  Walk / Flee
// ---------------------------------------------------------------------------

void PenguinOverlay::startWalk() {
  walkAnim_->start();
  bobAnim_->start();
  wingAnim_->start();
}

void PenguinOverlay::flee() {
  if (fleeing_) return;
  fleeing_ = true;

  bobAnim_->stop();
  wingAnim_->stop();

  // Speed up 3x and reverse direction
  walkAnim_->stop();
  walkAnim_->setDirection(fromLeft_ ? QAbstractAnimation::Backward
                                    : QAbstractAnimation::Forward);
  walkAnim_->setDuration(2000);
  walkAnim_->setEasingCurve(QEasingCurve::InQuad);
  walkAnim_->start();
}

void PenguinOverlay::onWalkFinished() {
  emit done();
  deleteLater();
}

// ---------------------------------------------------------------------------
//  Input events → flee!
// ---------------------------------------------------------------------------

void PenguinOverlay::enterEvent(QEvent*) {
  flee();
}

void PenguinOverlay::mousePressEvent(QMouseEvent*) {
  flee();
}

// ---------------------------------------------------------------------------
//  Paint
// ---------------------------------------------------------------------------

void PenguinOverlay::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Centre of drawing area
  const QPointF origin(50, 60);
  const qreal s = 1.0;  // scale factor

  drawPenguin(p, origin, s);
}

// ---------------------------------------------------------------------------
//  drawPenguin – geometric penguin with personality
// ---------------------------------------------------------------------------

void PenguinOverlay::drawPenguin(QPainter& p, const QPointF& o, qreal s) {
  const qreal dir = fromLeft_ ? 1.0 : -1.0;

  // ---- Helper lambdas ----
  auto rx = [&](qreal x) { return o.x() + x * s * dir; };
  auto ry = [&](qreal y) { return o.y() + y * s; };

  // ---- Shadow ----
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 30));
  p.drawEllipse(QPointF(o.x(), 54 * s), 18 * s, 4 * s);

  // ---- Body (white) ----
  p.setBrush(QColor(245, 245, 250));
  p.drawEllipse(QPointF(o.x(), ry(5)), 20 * s, 26 * s);

  // ---- Belly (lighter) ----
  p.setBrush(QColor(255, 255, 255));
  p.drawEllipse(QPointF(o.x(), ry(8)), 13 * s, 20 * s);

  // ---- Left wing (black, flapping) ----
  p.setBrush(QColor(30, 30, 35));
  p.save();
  p.translate(rx(-16), ry(4));
  p.rotate(wingAngle_ * dir * 0.6);
  p.drawEllipse(QPointF(0, 0), 5 * s, 16 * s);
  p.restore();

  // ---- Right wing (black, opposite flap) ----
  p.save();
  p.translate(rx(16), ry(4));
  p.rotate(-wingAngle_ * dir * 0.6);
  p.drawEllipse(QPointF(0, 0), 5 * s, 16 * s);
  p.restore();

  // ---- Head (black) ----
  p.setBrush(QColor(30, 30, 35));
  p.drawEllipse(QPointF(o.x(), ry(-18)), 13 * s, 12 * s);

  // ---- Eyes (white) ----
  p.setBrush(Qt::white);
  p.drawEllipse(QPointF(rx(-5), ry(-20)), 4 * s, 4.5 * s);
  p.drawEllipse(QPointF(rx(5), ry(-20)), 4 * s, 4.5 * s);

  // ---- Pupils (black) ----
  p.setBrush(QColor(20, 20, 25));
  qreal pupilOff = fromLeft_ ? 1.5 : -1.5;
  p.drawEllipse(QPointF(rx(-5) + pupilOff * s, ry(-19.5)), 2 * s, 2 * s);
  p.drawEllipse(QPointF(rx(5) + pupilOff * s, ry(-19.5)), 2 * s, 2 * s);

  // ---- Eye shine ----
  p.setBrush(QColor(255, 255, 255, 180));
  p.drawEllipse(QPointF(rx(-5) - 1.5 * s, ry(-21)), 1.2 * s, 1.2 * s);
  p.drawEllipse(QPointF(rx(5) - 1.5 * s, ry(-21)), 1.2 * s, 1.2 * s);

  // ---- Beak (orange) ----
  p.setBrush(QColor(255, 140, 30));
  QPolygonF beak;
  beak << QPointF(o.x(), ry(-14)) << QPointF(rx(10), ry(-12))
       << QPointF(o.x(), ry(-10));
  p.drawPolygon(beak);

  // ---- Feet (orange) ----
  p.setBrush(QColor(255, 165, 50));
  qreal footBob = qSin(walkPos_ * 0.05) * 2 * s;
  p.drawRoundedRect(
      QRectF(rx(-10) - 4 * s, ry(27) + footBob, 8 * s, 4 * s), 2, 2);
  p.drawRoundedRect(
      QRectF(rx(2) - 4 * s, ry(27) - footBob, 8 * s, 4 * s), 2, 2);

  // ---- Cheek blush (subtle pink) ----
  p.setBrush(QColor(255, 180, 180, 50));
  p.drawEllipse(QPointF(rx(-12), ry(-14)), 4 * s, 3 * s);
  p.drawEllipse(QPointF(rx(12), ry(-14)), 4 * s, 3 * s);
}
