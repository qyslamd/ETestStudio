#include "PixmapUtil.h"

#include <QPainter>

namespace etest {
namespace core {
namespace utils {

QPixmap PixmapUtil::grayOpacityImg(const QPixmap& pixmap) {
  QPixmap tempPixmap = pixmap;
  QPainter painter;
  painter.begin(&tempPixmap);
  painter.fillRect(pixmap.rect(), QColor(127, 127, 127, 127));
  painter.end();
  return tempPixmap;
}

}  // namespace utils
}  // namespace core
}  // namespace etest
