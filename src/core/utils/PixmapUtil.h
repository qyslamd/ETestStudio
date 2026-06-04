#ifndef ETEST_CORE_UTILS_PIXMAP_UTIL_H_
#define ETEST_CORE_UTILS_PIXMAP_UTIL_H_

#include <QPixmap>

namespace etest {
namespace core {
namespace utils {

class PixmapUtil {
 public:
  // 给图片叠加灰色半透明层
  static QPixmap grayOpacityImg(const QPixmap& pixmap);
};

}  // namespace utils
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_UTILS_PIXMAP_UTIL_H_
