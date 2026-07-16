#include "DockWidgetTabStyle.h"

#include "ThemeManager.h"

DockWidgetTabStyle::DockWidgetTabStyle(ads::CDockWidget* dock_widget,
                                       QWidget* parent)
    : ads::CDockWidgetTab(dock_widget) {
  // 阻止 QSS background 绘制，确保容器的梯形形状不被遮盖
  setAttribute(Qt::WA_StyledBackground, false);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_TranslucentBackground, true);

  // 主题切换时触发重绘
  connect(&etest::core_ui::ThemeManager::instance(),
          &etest::core_ui::ThemeManager::themeChanged, this,
          [this](bool) { onThemeChanged(); });
}

void DockWidgetTabStyle::paintEvent(QPaintEvent* event) {
  // 不绘制任何形状，形状由 DockAreaTabBarStyle::paintAllTabs 统一绘制
  // 不调 QFrame::paintEvent，避免 QSS background/border 绘制
  // 子控件（CElidingLabel、关闭按钮）各自独立收 paintEvent，不受影响
}

void DockWidgetTabStyle::onThemeChanged() {
  update();
}
