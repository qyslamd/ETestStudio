#include "AnimationDialog.h"

#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
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

  shadowEffect_ = new QGraphicsDropShadowEffect(widget_);
  shadowEffect_->setColor(QColor(105, 105, 105, 200));
  shadowEffect_->setBlurRadius(9);
  shadowEffect_->setOffset(0, 0);
  widget_->setGraphicsEffect(shadowEffect_);
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
  QPainterPath path;
  path.addRoundedRect(rect(), round_radius_, round_radius_);
  p.fillPath(path, QColor(205, 205, 205, 170));

  if (!cachedPixmap_.isNull()) {
    int x = (width() - cachedPixmap_.width()) / 2 + snapshotOffset_.x();
    int y = (height() - cachedPixmap_.height()) / 2 + snapshotOffset_.y();
    p.drawPixmap(x, y, cachedPixmap_);
  }
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
  auto center = QPoint(centerX - w / 2, centerY - h / 2);

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

  // 截图：关阴影 → grab → 恢复阴影
  if (shadowEffect_) shadowEffect_->setEnabled(false);
  cachedPixmap_ = widget_->grab();
  if (shadowEffect_) shadowEffect_->setEnabled(true);
  widget_->hide();

  auto* anime = new QVariantAnimation(this);
  anime->setEasingCurve(QEasingCurve::OutQuint);
  anime->setDuration(500);
  anime->setStartValue(QPoint(p1 - center));
  anime->setEndValue(QPoint(0, 0));

  connect(anime, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            snapshotOffset_ = value.toPoint();
            update();
          });
  connect(anime, &QVariantAnimation::finished, this, [this, center]() {
    snapshotOffset_ = {};
    cachedPixmap_ = {};
    widget_->move(center);
    widget_->show();
  });
  connect(anime, &QVariantAnimation::finished, anime,
          &QObject::deleteLater);
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
  auto center = QPoint(centerX - w / 2, centerY - h / 2);

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

  // 截图：关阴影 → grab → 恢复阴影
  if (shadowEffect_) shadowEffect_->setEnabled(false);
  cachedPixmap_ = widget_->grab();
  if (shadowEffect_) shadowEffect_->setEnabled(true);
  widget_->hide();

  auto* anime = new QVariantAnimation(this);
  anime->setEasingCurve(QEasingCurve::OutQuint);
  anime->setDuration(500);
  anime->setStartValue(QPoint(0, 0));
  anime->setEndValue(QPoint(p2 - center));

  connect(anime, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            snapshotOffset_ = value.toPoint();
            update();
          });
  connect(anime, &QVariantAnimation::finished, this, [this, func]() {
    snapshotOffset_ = {};
    cachedPixmap_ = {};
    emit hideAnimationFinished();
    if (func) func();
  });
  connect(anime, &QVariantAnimation::finished, anime,
          &QObject::deleteLater);
  anime->start();
}

}  // namespace etest::app
