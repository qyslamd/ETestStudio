#include "EyeWidget.h"

#include <QApplication>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QShowEvent>
#include <QTimer>
#include <QtMath>

EyeWidget::EyeWidget(QWidget* parent) : QWidget(parent) {
  qApp->installEventFilter(this);
  target_pos_ = QPointF(0, 0);
  smooth_pos_ = QPointF(0, 0);
  last_move_elapsed_.start();

  anim_timer_ = new QTimer(this);
  connect(anim_timer_, &QTimer::timeout, this, &EyeWidget::tick);
  // 定时器不默认启动，等 showEvent 时再启动

  blink_scheduler_ = new QTimer(this);
  blink_scheduler_->setSingleShot(true);
  connect(blink_scheduler_, &QTimer::timeout, this, [this]() {
    blink_phase_ = 0.001;
    blink_closing_ = true;
  });
  scheduleNextBlink();
}

void EyeWidget::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (!anim_timer_->isActive())
    anim_timer_->start(16);
}

void EyeWidget::hideEvent(QHideEvent* event) {
  QWidget::hideEvent(event);
  anim_timer_->stop();
}

void EyeWidget::tick() {
  bool needs_repaint = false;

  // smooth tracking
  smooth_pos_ += (target_pos_ - smooth_pos_) * 0.12;
  if (QLineF(smooth_pos_, target_pos_).length() > 0.5)
    needs_repaint = true;

  // idle → drowsy
  qint64 idle = last_move_elapsed_.elapsed();
  if (idle > 3000) {
    double old = drowsy_level_;
    drowsy_level_ = qMin(1.0, drowsy_level_ + 0.003);
    if (qAbs(drowsy_level_ - old) > 0.001)
      needs_repaint = true;
  }

  // cross-eyed animation
  if (cross_eye_phase_ > 0) {
    cross_eye_phase_ = qMax(0.0, cross_eye_phase_ - 0.025);
    needs_repaint = true;
  }

  // blink animation
  if (blink_phase_ > 0) {
    const double speed = 0.055;
    if (blink_closing_) {
      blink_phase_ = qMin(1.0, blink_phase_ + speed);
      if (blink_phase_ >= 1.0)
        blink_closing_ = false;
    } else {
      blink_phase_ = qMax(0.0, blink_phase_ - speed);
      if (blink_phase_ <= 0) {
        blink_phase_ = 0;
        blink_closing_ = true;
        if (blink_remaining_ > 0) {
          blink_remaining_--;
          if (blink_remaining_ > 0)
            blink_phase_ = 0.001;
        }
        if (blink_phase_ == 0)
          scheduleNextBlink();
      }
    }
    needs_repaint = true;
  }

  if (needs_repaint) {
    // mouse nearness for eyebrows
    QPointF center = rect().center();
    double dist = qMax(1.0, QLineF(smooth_pos_, center).length());
    double maxDist = qMax(width(), height()) * 0.7;
    mouse_nearness_ = qBound(0.0, 1.0 - dist / maxDist, 1.0);
    update();
  }
}

void EyeWidget::scheduleNextBlink() {
  if (blink_remaining_ > 0) return;
  int interval = QRandomGenerator::global()->bounded(2000, 5001);
  blink_scheduler_->start(interval);
}

// ---- color setters ----

void EyeWidget::setBgColor(const QColor& c) {
  bg_color_ = c;
  update();
}

void EyeWidget::setScleraColor(const QColor& c) {
  sclera_color_ = c;
  update();
}

void EyeWidget::setOutlineColor(const QColor& c) {
  outline_color_ = c;
  update();
}

void EyeWidget::setPupilColor(const QColor& c) {
  pupil_color_ = c;
  update();
}

void EyeWidget::setEyebrowColor(const QColor& c) {
  eyebrow_color_ = c;
  update();
}

void EyeWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // eye geometry
  double cx = width() / 2.0;
  double cy = height() / 2.0;
  double eyeR = qMin(width(), height()) / 5.0;
  double eyeSpacing = eyeR * 1.6;

  QPointF leftC(cx - eyeSpacing, cy);
  QPointF rightC(cx + eyeSpacing, cy);

  double closeFactor = qMin(1.0, blink_phase_ + drowsy_level_ * 0.6);
  double scaleY = 1.0 - closeFactor * 0.85;
  double maxPupilR = eyeR * 0.55;

  // pupil offset with cross-eyed effect
  auto rawOffset = [&](const QPointF& ec, double crossDir) {
    QPointF v(smooth_pos_.x() - ec.x(), smooth_pos_.y() - ec.y());
    v += QPointF(crossDir * cross_eye_phase_ * eyeSpacing * 0.25, 0);
    double len = qSqrt(v.x() * v.x() + v.y() * v.y());
    if (len <= maxPupilR) return v;
    return v * (maxPupilR / len);
  };
  QPointF leftOff = rawOffset(leftC, 1.0);
  QPointF rightOff = rawOffset(rightC, -1.0);

  // eyebrows
  double browRaise = -4 + mouse_nearness_ * 12 - drowsy_level_ * 5;
  auto drawBrow = [&](const QPointF& ec) {
    double bw = eyeR * 0.8;
    double by = ec.y() - eyeR * 1.1 + browRaise;
    QPainterPath path;
    path.moveTo(ec.x() - bw, by);
    path.quadTo(ec.x(), by - eyeR * 0.3 + browRaise * 0.3, ec.x() + bw, by);
    p.setPen(QPen(eyebrow_color_, 2.5));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
  };
  drawBrow(leftC);
  drawBrow(rightC);

  // eyes
  auto drawEye = [&](const QPointF& ec, const QPointF& off) {
    // eye white
    p.setBrush(sclera_color_);
    p.setPen(QPen(outline_color_, 2));
    p.drawEllipse(ec, eyeR, eyeR * scaleY);

    // pupil
    double pupilR = eyeR * 0.35 * qMax(0.3, scaleY);
    double pupilY = ec.y() + off.y() * scaleY - closeFactor * eyeR * 0.15;
    QPointF pupilPos(ec.x() + off.x(), pupilY);
    p.setBrush(pupil_color_);
    p.setPen(Qt::NoPen);
    p.drawEllipse(pupilPos, pupilR, pupilR * scaleY);
  };
  drawEye(leftC, leftOff);
  drawEye(rightC, rightOff);

  // eyelid line when partially closed
  if (closeFactor > 0.3) {
    double lineY = cy - closeFactor * eyeR * 0.2;
    p.setPen(QPen(outline_color_, 2));
    p.drawLine(QPointF(cx - eyeSpacing - eyeR, lineY),
               QPointF(cx + eyeSpacing + eyeR, lineY));
  }
}

bool EyeWidget::eventFilter(QObject* obj, QEvent* event) {
  if (!isVisible())
    return QWidget::eventFilter(obj, event);

  if (event->type() == QEvent::MouseMove) {
    auto* me = static_cast<QMouseEvent*>(event);
    target_pos_ = mapFromGlobal(me->globalPos());
    last_move_elapsed_.restart();
    drowsy_level_ = 0;
  } else if (event->type() == QEvent::MouseButtonPress) {
    if (QRandomGenerator::global()->bounded(2) == 0) {
      blink_remaining_ = 3;
      if (blink_phase_ <= 0) {
        blink_phase_ = 0.001;
        blink_closing_ = true;
        blink_scheduler_->stop();
      }
    } else {
      cross_eye_phase_ = 1.0;
    }
  }
  return QWidget::eventFilter(obj, event);
}
