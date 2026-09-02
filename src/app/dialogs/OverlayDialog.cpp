#include "OverlayDialog.h"

#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>

#include "core_ui/ThemeManager.h"

namespace etest::app {

namespace {

// 主题默认遮罩色：半透明黑，深色主题 alpha 微调（同
// GuidancePresentation::maskColor 的设计文档 3.8 口径）
QColor themeDefaultMaskColor() {
  const int alpha =
      etest::core_ui::ThemeManager::instance().isDarkTheme() ? 140 : 128;
  return QColor(0, 0, 0, alpha);
}

}  // namespace

OverlayDialog::OverlayDialog(QWidget* parent) : QDialog(parent) {
  setWindowFlag(Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  // 主题切换时重绘遮罩（SettingsDialog 自身即 OverlayDialog，内部切主题需实时跟随）
  connect(&etest::core_ui::ThemeManager::instance(),
          &etest::core_ui::ThemeManager::themeChanged, this,
          [this](bool) { update(); });
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
  if (widget_) {
    const int w = widget_->width();
    const int h = widget_->height();
    widget_->move((width() - w) / 2, (height() - h) / 2);
    widget_->show();
  }
}

void OverlayDialog::setMaskColor(const QColor& color) {
  mask_color_ = color;
  update();
}

QColor OverlayDialog::maskColor() const {
  return mask_color_.isValid() ? mask_color_ : themeDefaultMaskColor();
}

void OverlayDialog::paintEvent(QPaintEvent* event) {
  QDialog::paintEvent(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  QPainterPath path;
  path.addRoundedRect(rect(), round_radius_, round_radius_);
  p.fillPath(path, maskColor());
}

void OverlayDialog::keyPressEvent(QKeyEvent* e) {
  QWidget::keyPressEvent(e);
}

}  // namespace etest::app
