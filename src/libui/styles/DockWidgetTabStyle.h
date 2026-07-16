#ifndef ETEST_UI_DOCK_WIDGET_TAB_STYLE_H_
#define ETEST_UI_DOCK_WIDGET_TAB_STYLE_H_

#include "DockWidgetTab.h"

// Chrome 风格的 QADS Dock Tab 占位类。
// 形状由 DockAreaTabBarStyle::paintAllTabs 在容器层统一绘制，
// 本类的 paintEvent 留空，不绘制任何形状。
// 通过 setAttribute(WA_StyledBackground, false) 阻止 QSS 背景绘制，
// 确保容器的梯形形状不被子 tab 的纯色矩形遮盖。
class DockWidgetTabStyle : public ads::CDockWidgetTab {
  Q_OBJECT
 public:
  explicit DockWidgetTabStyle(ads::CDockWidget* dock_widget,
                              QWidget* parent = nullptr);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  void onThemeChanged();
};

#endif  // ETEST_UI_DOCK_WIDGET_TAB_STYLE_H_
