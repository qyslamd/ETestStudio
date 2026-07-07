#ifndef ETEST_UI_NO_FOCUS_RECT_STYLE_H_
#define ETEST_UI_NO_FOCUS_RECT_STYLE_H_

#include <QProxyStyle>

class NoFocusRectStyle : public QProxyStyle {
 public:
  void drawPrimitive(PrimitiveElement element,
                     const QStyleOption* option,
                     QPainter* painter,
                     const QWidget* widget) const override;
};

#endif  // ETEST_UI_NO_FOCUS_RECT_STYLE_H_
