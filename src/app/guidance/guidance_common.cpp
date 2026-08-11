#include "guidance_common.h"

#include <QGraphicsDropShadowEffect>
#include <QWidget>

namespace etest::app {

QGraphicsDropShadowEffect* createGraphicsShadow(QWidget* widget,
                                                const QColor& color,
                                                int blurRadius) {
  auto shadow = new QGraphicsDropShadowEffect(widget);
  shadow->setOffset(0, 0);
  shadow->setBlurRadius(blurRadius);
  shadow->setColor(color);
  widget->setGraphicsEffect(shadow);
  return shadow;
}

}  // namespace etest::app
