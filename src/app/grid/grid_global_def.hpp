#ifndef ETEST_APP_GRID_GLOBAL_DEF_H_
#define ETEST_APP_GRID_GLOBAL_DEF_H_

#include <QList>
#include <QPoint>
#include <QString>
#include <QtMath>
#include <cmath>

namespace etest::app::grid {

enum TileSpan {
  _0_0 = 0,
  _1_1 = 0x11,
  _1_2 = 0x12,
  _1_3 = 0x13,
  _1_4 = 0x14,
  _2_1 = 0x21,
  _2_2 = 0x22,
  _2_3 = 0x23,
  _2_4 = 0x24,
  _3_1 = 0x31,
  _3_2 = 0x32,
  _3_3 = 0x33,
  _3_4 = 0x34,
  _4_1 = 0x41,
  _4_2 = 0x42,
  _4_3 = 0x43,
  _4_4 = 0x44
};

inline constexpr auto FixedMargin = 24;
inline constexpr auto MinMargin = 10;
inline constexpr auto Radius = 8;

inline constexpr auto Width = 110;
inline constexpr auto Height = 110;

inline constexpr auto HSpacing = 10;
inline constexpr auto VSpacing = 10;

inline constexpr auto MimeType = "application/x-dndgriddata";

enum DragTrend {
  None = 0,
  TopCenter = 1,
  TopRight = 2,
  RightCenter,
  BottomRight,
  BottomCenter,
  BottomLeft,
  LeftCenter,
  TopLeft
};

inline DragTrend getDragingTrend(const QPoint& origin, const QPoint& dest) {
  auto point = dest - origin;
  auto a = std::atan2(point.y(), point.x());

  const auto _1_8_pi = M_PI / 8.0;

  if (a >= -_1_8_pi && a < _1_8_pi) return RightCenter;
  if (a >= _1_8_pi && a < _1_8_pi * 3) return BottomRight;
  if (a >= _1_8_pi * 3 && a < _1_8_pi * 5) return BottomCenter;
  if (a >= _1_8_pi * 5 && a < _1_8_pi * 7) return BottomLeft;
  if ((a >= _1_8_pi * 7 && a < M_PI) || (a >= -M_PI && a < -_1_8_pi * 7))
    return LeftCenter;
  if (a >= -_1_8_pi * 7 && a < -_1_8_pi * 5) return TopLeft;
  if (a >= -_1_8_pi * 5 && a < -_1_8_pi * 3) return TopCenter;
  if (a >= -_1_8_pi * 3 && a < -_1_8_pi) return TopRight;
  return None;
}

inline QString dragTrendToString(DragTrend trend) {
  switch (trend) {
    case TopCenter: return "TopCenter";
    case TopRight: return "TopRight";
    case RightCenter: return "RightCenter";
    case BottomRight: return "BottomRight";
    case BottomCenter: return "BottomCenter";
    case BottomLeft: return "BottomLeft";
    case LeftCenter: return "LeftCenter";
    case TopLeft: return "TopLeft";
    default: return "Invalid";
  }
}

inline void TypeGetRowCol(int type, int& rows, int& cols) {
  rows = 0; cols = 0;
  switch (type) {
    case _0_0: rows = 0; cols = 0; break;
    case _1_1: rows = 1; cols = 1; break;
    case _1_2: rows = 1; cols = 2; break;
    case _1_3: rows = 1; cols = 3; break;
    case _1_4: rows = 1; cols = 4; break;
    case _2_1: rows = 2; cols = 1; break;
    case _2_2: rows = 2; cols = 2; break;
    case _2_3: rows = 2; cols = 3; break;
    case _2_4: rows = 2; cols = 4; break;
    case _3_1: rows = 3; cols = 1; break;
    case _3_2: rows = 3; cols = 2; break;
    case _3_3: rows = 3; cols = 3; break;
    case _3_4: rows = 3; cols = 4; break;
    case _4_1: rows = 4; cols = 1; break;
    case _4_2: rows = 4; cols = 2; break;
    case _4_3: rows = 4; cols = 3; break;
    case _4_4: rows = 4; cols = 4; break;
    default: break;
  }
}

}  // namespace etest::app::grid

#endif  // ETEST_APP_GRID_GLOBAL_DEF_H_
