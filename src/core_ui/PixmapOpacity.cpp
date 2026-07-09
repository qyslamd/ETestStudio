#include "PixmapOpacity.h"

#include <QPainter>

namespace etest::core_ui {

QPixmap PixmapOpacity::grayOpacityImg(const QPixmap& pixmap) {
  QPixmap tempPixmap = pixmap;
  QPainter painter;
  painter.begin(&tempPixmap);
  painter.fillRect(pixmap.rect(), QColor(127, 127, 127, 127));
  painter.end();
  return tempPixmap;
}

}  // namespace etest::core_ui
