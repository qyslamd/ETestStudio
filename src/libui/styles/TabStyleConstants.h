#ifndef ETEST_UI_TAB_STYLE_CONSTANTS_H_
#define ETEST_UI_TAB_STYLE_CONSTANTS_H_

#include <QSize>

// 统一 tab 高度，TabBarStyle 和 DockAreaTabBarStyle 共用
inline constexpr int kTabBarHeight = 28;
inline constexpr QSize kMinTabSize(110, kTabBarHeight);

// tab 形状路径计算参数
inline constexpr qreal kTabTopMargin = 0.0;
inline constexpr qreal kTabHRatio = 1.0 / 5.0;

// 选中 tab 描边宽度（主色描边 + 暗色渐变外框 + 侧边延伸线共用）
inline constexpr int kTabBorderWidth = 1;

#endif  // ETEST_UI_TAB_STYLE_CONSTANTS_H_
