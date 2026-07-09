#ifndef ETEST_CORE_UI_PIXMAP_OPACITY_H_
#define ETEST_CORE_UI_PIXMAP_OPACITY_H_

#include <QPixmap>

namespace etest::core_ui {

class PixmapOpacity {
 public:
  // 给图片叠加灰色半透明层
  static QPixmap grayOpacityImg(const QPixmap& pixmap);
};

}  // namespace etest::core_ui

#endif  // ETEST_CORE_UI_PIXMAP_OPACITY_H_
