#include "OverlayDialog.h"

#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>

namespace etest::app {

OverlayDialog::OverlayDialog(QWidget* parent) : QDialog(parent) {
  setWindowFlag(Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
#ifndef Q_OS_WIN
  // Linux（WSLg/Wayland）下客户端拿不到全局坐标，独立顶层窗口无法覆盖
  // 到主窗口之上。退化为顶层窗口的子覆盖层，配合 showEvent 中 parentWidget()
  // 本地坐标定位（与 TuxSaverOverlay 同模式），跨平台一套逻辑。
  setWindowFlags((windowFlags() & ~Qt::WindowType_Mask) | Qt::Widget);
  // 创建者可能是侧边栏等子控件（如文件向导的父对象 ProjectStructureWidget），
  // 子覆盖层会被父对象边界裁剪、只能覆盖局部区域；统一 re-parent 到顶层窗口，
  // 保证遮罩覆盖整个主窗口。父对象本就是顶层窗口时（如登录/新建项目=MainWindow）
  // 不做任何改动。
  if (parent && parent->window() && parent->window() != parent) {
    setParent(parent->window());
  }
#endif
}

OverlayDialog::~OverlayDialog() = default;

void OverlayDialog::setWidget(QWidget* widget) {
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

void OverlayDialog::showEvent(QShowEvent* event) {
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
  // 直接居中显示卡片（去掉飞入动画，即时呈现）
  if (widget_) {
    const int w = widget_->width();
    const int h = widget_->height();
    widget_->move((width() - w) / 2, (height() - h) / 2);
    widget_->show();
  }
}

void OverlayDialog::paintEvent(QPaintEvent* event) {
  QDialog::paintEvent(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  QPainterPath path;
  path.addRoundedRect(rect(), round_radius_, round_radius_);
  p.fillPath(path, QColor(205, 205, 205, 170));
}

void OverlayDialog::keyPressEvent(QKeyEvent* e) {
  QWidget::keyPressEvent(e);
}

void OverlayDialog::actHideAnimation() {
  actHideAnimation([this] { accept(); });
}

void OverlayDialog::actHideAnimation(std::function<void()> func) {
  // 即时关闭（去掉飞出动画）
  if (widget_) {
    widget_->hide();
  }
  emit hideAnimationFinished();
  if (func) {
    func();
  }
}

}  // namespace etest::app
