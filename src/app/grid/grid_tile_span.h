#ifndef ETEST_APP_GRID_TILE_SPAN_H_
#define ETEST_APP_GRID_TILE_SPAN_H_

#include <QList>
#include <QString>

namespace etest::app::grid {

// 将 TileSpan 数值(如 0x12)转为十六进制字符串列表(如 ["1","2"])
inline QList<QString> decima2HexStringList(int num) {
  QList<QString> ret;
  auto str = QString::number(num, 16).toUpper();
  for (auto ch : str) ret << ch;
  return ret;
}

}  // namespace etest::app::grid

#endif  // ETEST_APP_GRID_TILE_SPAN_H_
