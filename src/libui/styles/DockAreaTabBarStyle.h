#ifndef ETEST_UI_DOCK_AREA_TAB_BAR_STYLE_H_
#define ETEST_UI_DOCK_AREA_TAB_BAR_STYLE_H_

#include <QColor>
#include <QLineF>
#include <QPainterPath>
#include <QBrush>

#include "DockAreaTabBar.h"

// Chrome 风格的 QADS DockAreaTabBar 自定义子类。
// 通过 eventFilter 拦截 tabsContainerWidget 的 Paint 事件，
// 用统一 painter 遍历所有子 tab 绘制梯形路径，实现与 TabBarStyle 一致的
// tab 间"熔合"效果（路径控制点可自由延伸到相邻 tab 区域）。
class DockAreaTabBarStyle : public ads::CDockAreaTabBar {
  Q_OBJECT
 public:
  explicit DockAreaTabBarStyle(ads::CDockAreaWidget* parent,
                                int tab_height = 28);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  enum class TabPosition { Beginning, Middle, End, OnlyOne };
  enum class SelectedPosition {
    NotAdjacent,
    NextIsSelected,
    PreviousIsSelected
  };

  void paintAllTabs(QPainter* painter);

  [[nodiscard]] TabPosition mapPosition(int index, int count) const;
  [[nodiscard]] SelectedPosition mapSelectedPosition(int index,
                                                     int count) const;

  // 路径计算（从 TabBarStyle 移植，参数改为 QRectF + TabPosition）
  QPainterPath selectedTabPath(const QRectF& r, TabPosition pos) const;
  QPainterPath hoveredTabPath(const QRectF& r, TabPosition pos,
                              SelectedPosition sel) const;
  QLineF dividingLine(const QRectF& r, TabPosition pos) const;

  // 色值辅助（从 TabBarStyle 移植）
  QBrush selectedBrush(const QRect& tab_rect) const;
  QColor hoveredColor() const;
  QColor dividerColor() const;
  QColor borderColor() const;

  void onThemeChanged();
  void applyViewportBackground();

  QWidget* tabs_container_ = nullptr;
  static constexpr qreal kTopMargin = 0.0;
  static constexpr qreal kHRatio = 1.0 / 5.0;
  bool dark_ = true;
};

#endif  // ETEST_UI_DOCK_AREA_TAB_BAR_STYLE_H_
