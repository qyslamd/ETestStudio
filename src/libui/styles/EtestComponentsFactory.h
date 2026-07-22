#ifndef ETEST_UI_ETEST_COMPONENTS_FACTORY_H_
#define ETEST_UI_ETEST_COMPONENTS_FACTORY_H_

#include "DockComponentsFactory.h"
#include "TabStyleConstants.h"

// 自定义 QADS 组件工厂。
// 重写 createDockWidgetTab() 返回 DockWidgetTabStyle（paintEvent 留空），
// 重写 createDockAreaTabBar() 返回 DockAreaTabBarStyle（容器级统一绘制）。
class EtestComponentsFactory : public ads::CDockComponentsFactory {
 public:
  explicit EtestComponentsFactory(int tab_height = kTabBarHeight);

  ads::CDockWidgetTab* createDockWidgetTab(
      ads::CDockWidget* dock_widget) const override;

  ads::CDockAreaTabBar* createDockAreaTabBar(
      ads::CDockAreaWidget* dock_area) const override;

 private:
  int tab_height_ = kTabBarHeight;
};

#endif  // ETEST_UI_ETEST_COMPONENTS_FACTORY_H_
