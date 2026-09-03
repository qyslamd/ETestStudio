#pragma once

#include <QColor>
#include <QHash>
#include <QString>

namespace etest::core_ui {

struct ThemePalette {
  QString themeId;
  bool isDark = true;

  // SARibbon 基础主题枚举值（int 避免依赖 SARibbon 头文件）
  // 0=Office2013, 1=Office2016Blue, 2=Office2021Blue, 3=Windows7, 4=Dark,
  // 5=Dark2
  int ribbonBaseTheme = 5;

  // 自定义 SARibbon QSS 路径（非空时加载覆盖内置主题颜色）
  QString ribbonQssPath;

  // 语义颜色
  QColor windowBackground;
  QColor panelBackground;
  QColor sceneBackground;  // QGraphicsScene 背景色
  QColor toolbarBackground;
  QColor hoverBackground;
  QColor selectionBackground;
  QColor tabSelectedBackground;
  QColor borderColor;
  QColor textColor;
  QColor secondaryTextColor;
  QColor disabledTextColor;
  QColor accentColor;
  QColor statusBarBackground;
  QColor clockFaceBackground;
  QColor clockHandColor;
  QColor clockSecondaryColor;
  QColor clockAccentColor;

  // 编辑器配色（key = CONFIG_EDITOR_* 常量, value = #RRGGBB 字符串）
  QHash<QString, QString> editorColors;
};

}  // namespace etest::core_ui
