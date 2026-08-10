#include "layout_calculator_v3.h"

#include <QtDebug>

#include "grid_tile.h"
#include "grid_layout.h"

namespace etest::app::grid {

LayoutCalculatorV3::LayoutCalculatorV3(GridLayout* p)
    : ILayoutCalculator(p) {}

LayoutCalculatorV3::~LayoutCalculatorV3() = default;

void LayoutCalculatorV3::calcBaseGridRects(const QRect& rect) {
  double verticalRemainHalf = 0.0, horizontalRemainHalf = 0.0;

  auto rows =
      calcLineGridCount(rect.height() - 2.0 * FixedMargin, Height, VSpacing,
                        verticalRemainHalf);
  auto columns =
      calcLineGridCount(rect.width() - 2.0 * FixedMargin, Width, HSpacing,
                        horizontalRemainHalf);

  // 设计规范固定 4 列
  if (columns > 4) {
    columns = 4;
    double used = columns * Width + (columns - 1) * HSpacing;
    double total = rect.width() - 2.0 * FixedMargin;
    horizontalRemainHalf = (total - used) / 2.0;
    if (horizontalRemainHalf < 0) horizontalRemainHalf = 0;
  }

  auto margin_top = FixedMargin + verticalRemainHalf;
  auto margin_left = FixedMargin + horizontalRemainHalf;

  layout_area_ = rect.adjusted(margin_left, margin_top, -margin_left,
                               -margin_top);

  // 更新基础网格
  base_grid_.clear();
  auto y = margin_top;
  for (auto i = 0; i < rows; i++) {
    auto x = margin_left;
    QVector<QPair<QRectF, QLayoutItem*>> oneCol;
    for (auto j = 0; j < columns; j++) {
      oneCol.append({QRectF(x, y, static_cast<qreal>(Width),
                            static_cast<qreal>(Height)),
                     nullptr});
      x += Width + HSpacing;
    }
    y += Height + VSpacing;
    base_grid_.append(std::move(oneCol));
  }

  // 更新拖放碰撞检测网格（扩大热区到间距一半）
  drag_use_grid_.clear();
  const auto _l = HSpacing / 2.0;
  const auto _t = VSpacing / 2.0;
  const auto _r = _l;
  const auto _b = _t;

  for (auto const& row : base_grid_) {
    QVector<QRectF> oneRow;
    for (auto const& pr : row) {
      oneRow << QRectF(pr.first).marginsAdded(QMarginsF(_l, _t, _r, _b));
    }
    drag_use_grid_.append(std::move(oneRow));
  }

  // 重建布局记录
  for (auto it = layout_map_.cbegin(); it != layout_map_.cend(); ++it) {
    auto info = it.value();
    setGridOccupied(info.start, info.row_span, info.col_span, it.key());
  }
}

void LayoutCalculatorV3::doLayout(const QRect& rect) {
  for (auto item : item_list_) {
    if (layout_map_.contains(item)) {
      auto posInfo = layout_map_[item];
      if (posInfo.valid()) {
        auto pos = posInfo.start;
        item->setGeometry(rectAt(pos.x(), pos.y()).toRect());
        continue;
      }
    }

    int rows = 0, columns = 0;
    calcGridsNeeded(item, rows, columns);

    auto pos = nextCanLayoutPos(rows, columns);
    if (pos.x() == -1 || pos.y() == -1) {
      item->setGeometry(QRect(rect.bottomRight(), item->sizeHint()));
    } else {
      auto r = rectAt(pos.x(), pos.y()).toRect();
      r.setSize(item->sizeHint());
      item->setGeometry(r);
      setGridOccupied(pos, rows, columns, item);
    }
    layout_map_[item] = {pos, rows, columns};
  }
}

QList<QRectF> LayoutCalculatorV3::dealWithDragMove(GridTile* widget,
                                                    const QPoint& pos,
                                                    DragTrend trend) {
  Q_UNUSED(trend)
  QList<QRectF> ret;

  auto key = getLayoutIem(item_list_, widget);
  if (!key) return ret;
  auto oldPosInfo = layout_map_[key];
  drag_cached_ = {key, oldPosInfo, {}};

  // 暂时解除拖动 item 的占用
  setGridOccupied(oldPosInfo.start, oldPosInfo.row_span, oldPosInfo.col_span,
                  nullptr);

  int row = -1, col = -1, row_span = 0, col_span = 0;
  indexOfDragRect(pos, row, col);
  if (row == -1 || col == -1) return ret;
  TypeGetRowCol(widget->type(), row_span, col_span);
  if (row_span == 0 || col_span == 0) return ret;

  QPoint okPos = findDragMoveOkPos({row, col}, row_span, col_span);
  if (okPos == QPoint(-1, -1)) return ret;

  PosInfo info{okPos, row_span, col_span};
  drag_cached_ = {key, oldPosInfo, info};
  return posInfoRects(info);
}

void LayoutCalculatorV3::dropApplied(bool ok) {
  if (ok) {
    auto item = std::get<0>(drag_cached_);
    auto oldPos = std::get<1>(drag_cached_);
    setGridOccupied(oldPos.start, oldPos.row_span, oldPos.col_span, nullptr);

    auto newPos = std::get<2>(drag_cached_);
    setGridOccupied(newPos.start, newPos.row_span, newPos.col_span, item);
    layout_map_[item] = newPos;
  } else {
    auto item = std::get<0>(drag_cached_);
    auto oldPos = std::get<1>(drag_cached_);
    setGridOccupied(oldPos.start, oldPos.row_span, oldPos.col_span, item);
  }
  drag_cached_ = {};
}

QRectF LayoutCalculatorV3::rectAtPoint(const QPoint& pos) const {
  for (int i = 0; i < base_grid_.count(); i++) {
    for (int j = 0; j < base_grid_[i].count(); j++) {
      if (base_grid_[i][j].first.contains(pos))
        return base_grid_[i][j].first;
    }
  }
  return QRectF();
}

QRectF LayoutCalculatorV3::rectAt(int row, int column) const {
  if (base_grid_.count() <= row) return QRectF();
  if (base_grid_.at(row).count() <= column) return QRectF();
  return base_grid_[row][column].first;
}

void LayoutCalculatorV3::indexOfRect(const QRectF& rect, int& row,
                                      int& column) const {
  row = -1;
  column = -1;
  for (int i = 0; i < base_grid_.count(); i++) {
    for (int j = 0; j < base_grid_[i].count(); j++) {
      if (base_grid_[i][j].first == rect) {
        row = i;
        column = j;
        return;
      }
    }
  }
}

void LayoutCalculatorV3::indexOfDragRect(const QPoint& pos, int& row,
                                          int& col) const {
  for (int i = 0; i < drag_use_grid_.count(); ++i) {
    for (int j = 0; j < drag_use_grid_[i].count(); j++) {
      if (drag_use_grid_[i][j].contains(pos)) {
        row = i;
        col = j;
        return;
      }
    }
  }
  row = -1;
  col = -1;
}

bool LayoutCalculatorV3::isNextGridCanLayout(const QPoint& startPos,
                                              int row_span,
                                              int col_span) const {
  if (startPos.x() < 0 || startPos.y() < 0) return false;
  for (int i = 0; i < row_span; i++) {
    int x = startPos.x() + i;
    if (x >= base_grid_.count()) return false;
    for (int j = 0; j < col_span; j++) {
      int y = startPos.y() + j;
      if (y >= base_grid_[x].count()) return false;
      if (base_grid_[x][y].second) return false;
    }
  }
  return true;
}

QPoint LayoutCalculatorV3::nextCanLayoutPos(int row_span,
                                             int column_span) const {
  if (row_span == 0 || column_span == 0) return QPoint(-1, -1);
  for (int i = 0; i < base_grid_.count(); i++) {
    for (int j = 0; j < base_grid_[i].count(); j++) {
      if (base_grid_[i][j].second) continue;
      if (isNextGridCanLayout(QPoint(i, j), row_span, column_span))
        return QPoint(i, j);
    }
  }
  return QPoint(-1, -1);
}

QPoint LayoutCalculatorV3::findDragMoveOkPos(const QPoint& pos,
                                              int row_span,
                                              int col_span) const {
  if (row_span == 0 || col_span == 0) return QPoint(-1, -1);

  auto tempMakeList = [](int start, int span) -> QList<int> {
    QList<int> ret;
    for (int i = 0; i < span; i++) ret << start + i;
    return ret;
  };

  // 找到可用位置且包含鼠标落点
  for (int i = 0; i < base_grid_.count(); i++) {
    for (int j = 0; j < base_grid_[i].count(); j++) {
      if (base_grid_[i][j].second) continue;
      if (isNextGridCanLayout(QPoint(i, j), row_span, col_span)) {
        if (tempMakeList(i, row_span).contains(pos.x()) &&
            tempMakeList(j, col_span).contains(pos.y())) {
          return QPoint(i, j);
        }
      }
    }
  }
  return QPoint(-1, -1);
}

void LayoutCalculatorV3::setGridOccupied(const QPoint& start, int rows,
                                          int columns, QLayoutItem* item) {
  if (start.x() < 0 || start.y() < 0) return;
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      auto row = start.x() + i;
      auto col = start.y() + j;
      if (base_grid_.count() <= row) continue;
      if (base_grid_.at(row).count() <= col) continue;
      base_grid_[row][col].second = item;
    }
  }
}

QList<QRectF> LayoutCalculatorV3::posInfoRects(const PosInfo& info) const {
  QList<QRectF> ret;
  if (info.start.x() < 0 || info.start.y() < 0) return ret;
  for (int i = 0; i < info.row_span; i++) {
    for (int j = 0; j < info.col_span; j++) {
      ret << rectAt(info.start.x() + i, info.start.y() + j);
    }
  }
  return ret;
}

QList<QRectF> LayoutCalculatorV3::baseGridRects() const {
  QList<QRectF> result;
  for (const auto& vec : base_grid_) {
    for (const auto& pr : vec) {
      result << pr.first;
    }
  }
  return result;
}

QList<QRectF> LayoutCalculatorV3::dargJudgeGridRects() const {
  QList<QRectF> result;
  for (const auto& vec : drag_use_grid_) {
    for (const auto& r : vec) {
      result.append(r);
    }
  }
  return result;
}

QList<QPair<QRectF, bool>> LayoutCalculatorV3::layoutedGridRects() const {
  QList<QPair<QRectF, bool>> result;
  for (const auto& vec : base_grid_) {
    for (const auto& p : vec) {
      result << p;
    }
  }
  return result;
}

}  // namespace etest::app::grid
