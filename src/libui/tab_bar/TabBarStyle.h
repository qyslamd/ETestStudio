#ifndef ETEST_UI_TAB_BAR_STYLE_H_
#define ETEST_UI_TAB_BAR_STYLE_H_

#include <QProxyStyle>

// Chrome 风格的圆角 Tab 自绘样式（参考 draw_tab_shape demo）
class TabBarStyle : public QProxyStyle {
 public:
  TabBarStyle();

  // QStyle interface
 public:
  QSize sizeFromContents(ContentsType type,
                         const QStyleOption* option,
                         const QSize& size,
                         const QWidget* widget) const override;
  void drawControl(ControlElement element,
                   const QStyleOption* opt,
                   QPainter* p,
                   const QWidget* w) const override;

 private:
  void drawTabBarTabShape(const QStyleOption* option,
                          QPainter* painter,
                          const QWidget* w) const;
  QPainterPath getSelectedShape(const QStyleOption* option) const;
  QPainterPath getHoveredShape(const QStyleOption* option) const;
  QLineF getDividingLine(const QStyleOption* option) const;

 private:
  const qreal topMargin = 0.0;
  const qreal HRatio = 1.0 / 5.0;
};

#endif  // ETEST_UI_TAB_BAR_STYLE_H_
