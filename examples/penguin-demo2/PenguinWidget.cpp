#include "PenguinWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QtMath>
#include <QPainterPath>

// ============================================================================
//  Construction
// ============================================================================

PenguinWidget::PenguinWidget(QWidget* parent) : QWidget(parent) {
  // Transparent background — we draw the penguin with alpha
  setAttribute(Qt::WA_TranslucentBackground);
  setMouseTracking(true);
  resize(80, 96);

  // Animation tick (50 ms → ~20 fps)
  anim_timer_ = new QTimer(this);
  connect(anim_timer_, &QTimer::timeout, this, &PenguinWidget::tick);

  // Pause between walks
  pause_timer_ = new QTimer(this);
  pause_timer_->setSingleShot(true);

  // Blink timer (random interval)
  blink_timer_ = new QTimer(this);
  blink_timer_->setSingleShot(true);
  connect(blink_timer_, &QTimer::timeout, this, [this]() {
    blink_value_ = 0.001;
    blink_closing_ = true;
  });
}

PenguinWidget::~PenguinWidget() = default;

// ============================================================================
//  Public API
// ============================================================================

void PenguinWidget::appear() {
  if (!parentWidget()) return;
  stage_size_ = parentWidget()->size();

  // Random start position well within bounds
  int margin = 30;
  int cx = QRandomGenerator::global()->bounded(margin, static_cast<int>(stage_size_.width()) - margin);
  int cy = QRandomGenerator::global()->bounded(margin, static_cast<int>(stage_size_.height()) - margin);
  pos_ = QPointF(cx, cy);

  qreal hw = width() / 2.0, hh = height() / 2.0;
  move(static_cast<int>(cx - hw), static_cast<int>(cy - hh));
  show();
  raise();

  state_ = IDLE;
  walk_phase_ = 0;
  walk_speed_ = 1.0;

  // Start with a walk
  pickNewTarget();
  state_ = WALKING;
  anim_timer_->start(50);

  scheduleNextBlink();
}

void PenguinWidget::dismiss() {
  if (state_ == FLEEING || state_ == HIDDEN) return;
  startFlee();
}

QString PenguinWidget::stateName() const {
  switch (state_) {
    case IDLE:    return QStringLiteral("休息中");
    case WALKING: return QStringLiteral("散步中");
    case FLEEING: return QStringLiteral("逃跑啦！");
    default:      return QString();
  }
}

// ============================================================================
//  Events
// ============================================================================

void PenguinWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  QPointF center(width() / 2.0, height() / 2.0 + 4);

  if (state_ == HIDDEN) return;

  // Walking direction
  qreal dir = 1.0;
  if (state_ == WALKING || state_ == FLEEING) {
    dir = (target_.x() >= pos_.x()) ? 1.0 : -1.0;
  }

  // Convert walk phase to body bob
  qreal bob = qSin(walk_phase_) * 2.5;

  p.save();
  p.translate(center.x(), center.y() + bob);
  drawPenguin(p, QPointF(0, 0), dir);
  p.restore();
}

void PenguinWidget::mousePressEvent(QMouseEvent*) {
  startFlee();
}

void PenguinWidget::enterEvent(QEvent*) {
  // Look toward mouse — already happens naturally via walk direction
  // Could add a head tilt effect here later
}

// ============================================================================
//  Animation tick
// ============================================================================

void PenguinWidget::tick() {
  if (state_ == HIDDEN) return;

  // Update stage size cache
  if (parentWidget()) stage_size_ = parentWidget()->size();

  // ---- Blink animation ----
  if (blink_value_ > 0) {
    const double speed = 0.08;
    if (blink_closing_) {
      blink_value_ = qMin(1.0, blink_value_ + speed);
      if (blink_value_ >= 1.0) blink_closing_ = false;
    } else {
      blink_value_ = qMax(0.0, blink_value_ - speed);
      if (blink_value_ <= 0) {
        blink_value_ = 0;
        blink_closing_ = true;
        scheduleNextBlink();
      }
    }
  }

  // ---- Walk / flee movement ----
  if (state_ == WALKING || state_ == FLEEING) {
    QPointF delta = target_ - pos_;
    qreal dist = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    qreal step = (state_ == FLEEING ? 8.0 : 1.8) * walk_speed_;

    if (dist < step) {
      pos_ = target_;
      if (state_ == FLEEING) {
        // Reached edge — done
        state_ = HIDDEN;
        anim_timer_->stop();
        hide();
        emit dismissed();
        deleteLater();
        return;
      }
      // Arrived — pause
      state_ = IDLE;
      walk_phase_ = 0;
      int pauseMs = QRandomGenerator::global()->bounded(1500, 4000);
      pause_timer_->start(pauseMs);
    } else {
      pos_ += delta * (step / dist);
      walk_phase_ += 0.12 * walk_speed_;
    }

    // Clamp to stage bounds
    qreal hw = width() / 2.0, hh = height() / 2.0;
    pos_.setX(qBound(hw, pos_.x(), stage_size_.width() - hw));
    pos_.setY(qBound(hh, pos_.y(), stage_size_.height() - hh));

    move(static_cast<int>(pos_.x() - hw), static_cast<int>(pos_.y() - hh));
  }

  // ---- Pause timer expired → walk again ----
  if (state_ == IDLE && !pause_timer_->isActive()) {
    pickNewTarget();
    state_ = WALKING;
  }

  update();
}

void PenguinWidget::pickNewTarget() {
  if (!parentWidget()) return;
  int margin = 30;
  int cx = QRandomGenerator::global()->bounded(margin, static_cast<int>(stage_size_.width()) - margin);
  int cy = QRandomGenerator::global()->bounded(margin, static_cast<int>(stage_size_.height()) - margin);
  target_ = QPointF(cx, cy);
}

void PenguinWidget::startFlee() {
  if (state_ == FLEEING || state_ == HIDDEN) return;
  state_ = FLEEING;
  walk_speed_ = 4.0;
  pause_timer_->stop();

  // Flee toward the nearest edge
  qreal hw = width() / 2.0, hh = height() / 2.0;
  qreal ex = pos_.x();
  qreal ey = pos_.y();

  // Pick a point well outside the stage
  qreal distLeft = ex;
  qreal distRight = stage_size_.width() - ex;
  qreal distTop = ey;
  qreal distBottom = stage_size_.height() - ey;

  if (distLeft < distRight) {
    target_.setX(-width());
  } else {
    target_.setX(stage_size_.width() + width());
  }
  if (distTop < distBottom) {
    target_.setY(-height());
  } else {
    target_.setY(stage_size_.height() + height());
  }
}

// ============================================================================
//  Blink scheduler
// ============================================================================

void PenguinWidget::scheduleNextBlink() {
  int interval = QRandomGenerator::global()->bounded(2000, 6000);
  blink_timer_->start(interval);
}

// ============================================================================
//  drawPenguin — cute Tux-style penguin
// ============================================================================

void PenguinWidget::drawPenguin(QPainter& p, const QPointF& c, qreal dir) {
  // ── Colors ──
  const QColor bodyClr(0x22, 0x22, 0x28);
  const QColor bellyClr(0xFF, 0xFF, 0xFF);
  const QColor faceClr(0xF5, 0xF5, 0xF5);
  const QColor beakClr(0xFF, 0x8C, 0x00);
  const QColor footClr(0xFF, 0xA0, 0x30);
  const QColor eyeClr(0x1A, 0x1A, 0x1A);
  const QColor shineClr(255, 255, 255, 200);
  const QColor shadowClr(0, 0, 0, 45);

  p.setPen(Qt::NoPen);

  // ── Shadow ──
  p.setBrush(shadowClr);
  p.drawEllipse(QPointF(c.x(), c.y() + 30), 18, 4);

  // ── Feet (animated) ──
  qreal footAlt = qSin(walk_phase_) * 3.0;
  p.setBrush(footClr);
  auto drawFoot = [&](qreal x, qreal yOff) {
    QRectF rf(c.x() + x - 6, c.y() + 24 + yOff, 12, 6);
    p.drawRoundedRect(rf, 3, 3);
  };
  drawFoot(-10, footAlt);
  drawFoot(10, -footAlt);

  // ── Body (pear-shaped: wider lower body) ──
  p.setBrush(bodyClr);
  // Upper body (slightly narrower)
  p.drawEllipse(QPointF(c.x(), c.y() - 2), 15, 18);
  // Lower body (wider, creating pear shape)
  p.drawEllipse(QPointF(c.x(), c.y() + 8), 18, 14);

  // ── Belly (tall white patch) ──
  p.setBrush(bellyClr);
  QPainterPath bellyPath;
  bellyPath.addEllipse(QPointF(c.x(), c.y() + 4), 11, 18);
  p.drawPath(bellyPath);

  // ── Wings (animated, drawn behind body overlay) ──
  qreal wingSw = qSin(walk_phase_ * 0.6) * 10.0;
  p.setBrush(bodyClr);
  auto drawWing = [&](qreal x, qreal angle) {
    p.save();
    p.translate(c.x() + x, c.y() + 2);
    p.rotate(angle);
    p.drawEllipse(QPointF(0, 0), 6, 16);
    p.restore();
  };
  drawWing(-17, -wingSw + 8);
  drawWing(17, wingSw - 8);

  // ── Head (round, slightly overlapping body) ──
  p.setBrush(bodyClr);
  p.drawEllipse(QPointF(c.x(), c.y() - 26), 15, 14);

  // ── Face (white patch on front of head) ──
  p.setBrush(faceClr);
  p.drawEllipse(QPointF(c.x(), c.y() - 25), 11, 10);

  // ── Blush (subtle pink cheeks) ──
  p.setBrush(QColor(255, 180, 180, 55));
  p.drawEllipse(QPointF(c.x() - 9, c.y() - 22), 5, 4);
  p.drawEllipse(QPointF(c.x() + 9, c.y() - 22), 5, 4);

  // ── Eyes ──
  bool eyesClosed = (blink_value_ > 0.5);
  if (eyesClosed) {
    p.setPen(QPen(eyeClr, 2));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(c.x() - 7, c.y() - 27), QPointF(c.x() - 2.5, c.y() - 27));
    p.drawLine(QPointF(c.x() + 2.5, c.y() - 27), QPointF(c.x() + 7, c.y() - 27));
  } else {
    p.setPen(Qt::NoPen);
    p.setBrush(eyeClr);
    // Big round eyes
    p.drawEllipse(QPointF(c.x() - 5.5, c.y() - 26.5), 3.5, 3.5);
    p.drawEllipse(QPointF(c.x() + 5.5, c.y() - 26.5), 3.5, 3.5);
    // Big highlights (kawaii!)
    p.setBrush(shineClr);
    p.drawEllipse(QPointF(c.x() - 6.5, c.y() - 28), 2, 2);
    p.drawEllipse(QPointF(c.x() + 4.5, c.y() - 28), 2, 2);
    // Small secondary highlight
    p.drawEllipse(QPointF(c.x() - 4, c.y() - 25), 0.8, 0.8);
    p.drawEllipse(QPointF(c.x() + 7, c.y() - 25), 0.8, 0.8);
  }

  // ── Beak (cute rounded triangle) ──
  QPainterPath beakPath;
  beakPath.moveTo(c.x() - 5, c.y() - 24);
  beakPath.lineTo(c.x() + 5, c.y() - 24);
  beakPath.quadTo(c.x() + 5, c.y() - 19, c.x(), c.y() - 18);
  beakPath.quadTo(c.x() - 5, c.y() - 19, c.x() - 5, c.y() - 24);
  p.setBrush(beakClr);
  p.setPen(Qt::NoPen);
  p.drawPath(beakPath);

  // ── Little smile ──
  p.setPen(QPen(bodyClr, 1.2));
  QPainterPath smile;
  smile.moveTo(c.x() - 3, c.y() - 17);
  smile.quadTo(c.x(), c.y() - 15, c.x() + 3, c.y() - 17);
  p.setBrush(Qt::NoBrush);
  p.drawPath(smile);
}
