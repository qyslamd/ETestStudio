#include "EtestComponentsFactory.h"

#include "DockAreaTabBarStyle.h"
#include "DockWidgetTabStyle.h"

EtestComponentsFactory::EtestComponentsFactory(int tab_height)
    : tab_height_(tab_height) {}

ads::CDockWidgetTab* EtestComponentsFactory::createDockWidgetTab(
    ads::CDockWidget* dock_widget) const {
  return new DockWidgetTabStyle(dock_widget);
}

ads::CDockAreaTabBar* EtestComponentsFactory::createDockAreaTabBar(
    ads::CDockAreaWidget* dock_area) const {
  return new DockAreaTabBarStyle(dock_area, tab_height_);
}
