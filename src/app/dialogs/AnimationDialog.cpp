#include "AnimationDialog.h"

#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QShowEvent>
#include <QVariantAnimation>

#include "utils/window_mover.h"

namespace etest::app {

AnimationDialog::AnimationDialog(QWidget* parent) : QDialog(parent) {
  setWindowFlag(Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
}

AnimationDialog::~AnimationDialog() = default;

void AnimationDialog::removeWindowMover() {
  if (mover_) {
    mover_->deleteLater();
  }
}

void AnimationDialog::setWidget(QWidget* widget) {
  if (widget_) {
    widget_->deleteLater();
  }
  widget_ = widget;
  widget_->setParent(this);
  mover_ = new WindowMover(widget_, this);

  auto* shadow = new QGraphicsDropShadowEffect(widget_);
  shadow->setColor(QColor(105, 105, 105, 200));
  shadow->setBlurRadius(9);
  shadow->setOffset(0, 0);
  widget_->setGraphicsEffect(shadow);
}

void AnimationDialog::showEvent(QShowEvent* event) {
  if (parentWidget()) {
    auto* top = parentWidget()->window();
    setGeometry(top->geometry());
  }
  actShowAnimation();
}

void AnimationDialog::paintEvent(QPaintEvent* event) {
  QDialog::paintEvent(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.save();
  p.fillRect(this->rect(), QColor(205, 205, 205, 170));
  p.restore();
}

void AnimationDialog::keyPressEvent(QKeyEvent* e) {
  QWidget::keyPressEvent(e);
}

void AnimationDialog::actShowAnimation() {
  if (!widget_) {
    return;
  }
  auto centerX = this->width() / 2;
  auto centerY = this->height() / 2;
  auto w = widget_->width();
  auto h = widget_->height();
  QPoint p1;

  switch ((quint32)QRandomGenerator::global()->generate() % 4) {
    case 0:
      p1 = QPoint(-w, centerY - h / 2);
      break;
    case 1:
      p1 = QPoint(centerX - w / 2, -h);
      break;
    case 2:
      p1 = QPoint(this->width(), centerY - h / 2);
      break;
    case 3:
      p1 = QPoint(centerX - w / 2, this->height());
      break;
    default:
      break;
  }

  auto* anime = new QPropertyAnimation(this);
  anime->setEasingCurve(QEasingCurve::OutQuint);
  anime->setTargetObject(widget_);
  anime->setPropertyName("pos");
  anime->setDuration(500);
  anime->setStartValue(p1);
  anime->setEndValue(QPoint(centerX - w / 2, centerY - h / 2));

  connect(anime, &QVariantAnimation::finished, anime, &QObject::deleteLater);
  anime->start();
}

void AnimationDialog::actHideAnimation() {
  actHideAnimation([this] { accept(); });
}

void AnimationDialog::actHideAnimation(std::function<void()> func) {
  if (!widget_) {
    return;
  }
  auto centerX = this->width() / 2;
  auto centerY = this->height() / 2;
  auto w = widget_->width();
  auto h = widget_->height();

  QPoint p2;
  switch ((quint32)QRandomGenerator::global()->generate() % 4) {
    case 0:
      p2 = QPoint(-w, centerY - h / 2);
      break;
    case 1:
      p2 = QPoint(centerX - w / 2, -h);
      break;
    case 2:
      p2 = QPoint(this->width(), centerY - h / 2);
      break;
    case 3:
      p2 = QPoint(centerX - w / 2, this->height());
      break;
    default:
      break;
  }

  auto* anime = new QPropertyAnimation(this);
  anime->setEasingCurve(QEasingCurve::OutQuint);
  anime->setTargetObject(widget_);
  anime->setPropertyName("pos");
  anime->setDuration(500);
  anime->setStartValue(widget_->pos());
  anime->setEndValue(p2);
  if (func) {
    connect(anime, &QPropertyAnimation::finished, this, [=] { func(); });
  }
  connect(anime, &QPropertyAnimation::finished, this,
          &AnimationDialog::hideAnimationFinished);
  connect(anime, &QVariantAnimation::finished, anime, &QObject::deleteLater);
  anime->start();
}

}  // namespace etest::app
