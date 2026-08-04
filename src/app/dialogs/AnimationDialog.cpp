#include "AnimationDialog.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QScreen>
#include <QShowEvent>
#include <QVariantAnimation>

namespace etest::app {

AnimationDialog::AnimationDialog(QWidget* parent) : QDialog(parent) {
  setWindowFlag(Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
#ifndef Q_OS_WIN
  // Linux（WSLg/Wayland）下客户端拿不到全局坐标，独立顶层窗口无法覆盖
  // 到主窗口之上。退化为父窗口的子覆盖层，配合 showEvent 中 parentWidget()
  // 本地坐标定位（与 TuxSaverOverlay 同模式），跨平台一套逻辑。
  setWindowFlags((windowFlags() & ~Qt::WindowType_Mask) | Qt::Widget);
#endif
}

AnimationDialog::~AnimationDialog() = default;

void AnimationDialog::setWidget(QWidget* widget) {
  if (widget_) {
    widget_->deleteLater();
  }
  widget_ = widget;
  widget_->setParent(this);

  shadowEffect_ = new QGraphicsDropShadowEffect(widget_);
  shadowEffect_->setColor(QColor(105, 105, 105, 200));
  shadowEffect_->setBlurRadius(9);
  shadowEffect_->setOffset(0, 0);
  widget_->setGraphicsEffect(shadowEffect_);
}

void AnimationDialog::showEvent(QShowEvent* event) {
  if (parentWidget()) {
#ifdef Q_OS_WIN
    // Win32 可查询全局窗口矩形，遮罩覆盖主窗口真实屏幕位置
    setGeometry(parentWidget()->window()->geometry());
#else
    // Wayland 客户端拿不到全局坐标，遮罩已退化为父窗口子覆盖层，
    // 用本地坐标覆盖父窗口客户区，父窗口移动/缩放时自动跟随
    setGeometry(parentWidget()->rect());
#endif
  }
  raise();
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
  if (shadowEffect_)
    shadowEffect_->setEnabled(false);
  cachedPixmap_ = widget_->grab();
  if (shadowEffect_)
    shadowEffect_->setEnabled(true);
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
  if (shadowEffect_)
    shadowEffect_->setEnabled(false);
  cachedPixmap_ = widget_->grab();
  if (shadowEffect_)
    shadowEffect_->setEnabled(true);
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
    if (func)
      func();
  });
  connect(anime, &QVariantAnimation::finished, anime, &QObject::deleteLater);
  anime->start();
}

}  // namespace etest::app
