#include "NoFocusRectStyle.h"
#include <QApplication>
#include <QPainter>

void NoFocusRectStyle::drawPrimitive(PrimitiveElement element,
                                     const QStyleOption* option,
                                     QPainter* painter,
                                     const QWidget* widget) const {
  if (element == PE_FrameFocusRect)
    return;
  QProxyStyle::drawPrimitive(element, option, painter, widget);
}
