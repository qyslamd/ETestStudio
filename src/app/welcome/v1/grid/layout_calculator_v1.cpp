#include "layout_calculator_v1.h"

#include <QLayoutItem>
#include <QtDebug>

#include "grid_tile.h"
#include "grid_layout.h"

namespace etest::app::grid {

LayoutCalculatorV1::LayoutCalculatorV1(GridLayout* p)
    : ILayoutCalculator(p) {}

LayoutCalculatorV1::~LayoutCalculatorV1() { base_grid_.clear(); }

int LayoutCalculatorV1::heightForWidth(int) const { return -1; }

void LayoutCalculatorV1::calcBaseGridRects(const QRect& rect) {
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
    // 重新计算水平居中偏移
    double used = columns * Width + (columns - 1) * HSpacing;
    double total = rect.width() - 2.0 * FixedMargin;
    horizontalRemainHalf = (total - used) / 2.0;
    if (horizontalRemainHalf < 0) horizontalRemainHalf = 0;
  }

  auto margin_top = FixedMargin + verticalRemainHalf;
  auto margin_left = FixedMargin + horizontalRemainHalf;

  layout_area_ = rect.adjusted(margin_left, margin_top, -margin_left,
                               -margin_top);

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
}

void LayoutCalculatorV1::doLayout(const QRect& rect) {
  for (auto item : item_list_) {
    int rows = 0, columns = 0;
    calcGridsNeeded(item, rows, columns);

    auto pos = nextAvailableGridPos(base_grid_, rows, columns);
    if (pos.x() == -1 || pos.y() == -1) {
      item->setGeometry(QRect(rect.bottomRight(), item->sizeHint()));
    } else {
      auto r = rectAt(pos.x(), pos.y()).toRect();
      r.setSize(item->sizeHint());
      item->setGeometry(r);
      setGridOccupied(base_grid_, pos, rows, columns, item);
    }
  }
}

QList<QRectF> LayoutCalculatorV1::dealWithDragMove(GridTile* item,
                                                   const QPoint& pos,
                                                   DragTrend trend) {
  Q_UNUSED(trend)

  QList<QRectF> ret;

  auto itemListCopy = item_list_;
  auto baseGridCopy = base_grid_;

  auto dragItem = getLayoutIem(itemListCopy, item);
  if (!dragItem) return ret;

  QLayoutItem* targetItem = layoutedItemAt(baseGridCopy, pos);
  if (!targetItem) return ret;
  if (dragItem == targetItem) return ret;

  // 获取拖拽项和目标项的位置
  int dragIndex = itemListCopy.indexOf(dragItem);
  int targetIndex = itemListCopy.indexOf(targetItem);

  // 把拖拽项移到目标位置
  itemListCopy.takeAt(dragIndex);
  itemListCopy.insert(targetIndex, dragItem);

  // 重新布局计算
  resetGridOccupied(baseGridCopy, {});
  QRect r = p_->geometry();

  for (int i = 0; i < itemListCopy.count(); i++) {
    auto it = itemListCopy.at(i);
    int rows = 0, columns = 0;
    calcGridsNeeded(it, rows, columns);

    auto p = nextAvailableGridPos(baseGridCopy, rows, columns);
    if (p.x() == -1 || p.y() == -1) {
      it->setGeometry(QRect(r.bottomRight(), it->sizeHint()));
    } else {
      auto rr = rectAt(p.x(), p.y()).toRect();
      rr.setSize(it->sizeHint());
      it->setGeometry(rr);
      setGridOccupied(baseGridCopy, p, rows, columns, it);
    }

    if (targetIndex == i) {
      ret = getRects(baseGridCopy, p, rows, columns);
    }
  }

  drag_cached_item_list_ = itemListCopy;
  drag_cached_base_grid_ = baseGridCopy;

  return ret;
}

void LayoutCalculatorV1::dropApplied(bool ok) {
  if (ok) {
    item_list_ = drag_cached_item_list_;
    base_grid_ = drag_cached_base_grid_;

    drag_cached_item_list_.clear();
    drag_cached_base_grid_.clear();
  } else {
    p_->update();
  }

  // 更新磁贴序号（调试用）
  {
    int i = 1;
    for (auto item : item_list_) {
      if (auto lb = qobject_cast<GridTile*>(item->widget())) {
        lb->setNameText(QString::number(i));
      }
      i++;
    }
  }
}

QRectF LayoutCalculatorV1::rectAtPoint(const QPoint& pos) const {
  for (int i = 0; i < base_grid_.count(); i++) {
    for (int j = 0; j < base_grid_[i].count(); j++) {
      if (base_grid_[i][j].first.contains(pos))
        return base_grid_[i][j].first;
    }
  }
  return QRectF();
}

QRectF LayoutCalculatorV1::rectAt(int row, int column) const {
  if (base_grid_.count() <= row) return QRectF();
  if (base_grid_.at(row).count() <= column) return QRectF();
  return base_grid_[row][column].first;
}

void LayoutCalculatorV1::indexOfRect(const QRectF& rect, int& row,
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

bool LayoutCalculatorV1::isNextGridsAvailable(const GridTable& baseGrid,
                                              const QPoint& posStart,
                                              int row_span, int col_span) {
  for (int i = 0; i < row_span; i++) {
    int x = posStart.x() + i;
    if (x >= baseGrid.count()) return false;
    for (int j = 0; j < col_span; j++) {
      int y = posStart.y() + j;
      if (y >= baseGrid[x].count()) return false;
      if (baseGrid[x][y].second) return false;
    }
  }
  return true;
}

QPoint LayoutCalculatorV1::nextAvailableGridPos(const GridTable& baseGrid,
                                                int row_span,
                                                int column_span) {
  if (row_span == 0 || column_span == 0) return QPoint(-1, -1);
  for (int i = 0; i < baseGrid.count(); i++) {
    for (int j = 0; j < baseGrid[i].count(); j++) {
      if (baseGrid[i][j].second) continue;
      if (isNextGridsAvailable(baseGrid, QPoint(i, j), row_span, column_span))
        return QPoint(i, j);
    }
  }
  return QPoint(-1, -1);
}

QList<QRectF> LayoutCalculatorV1::getRects(const GridTable& baseGrid,
                                           const QPoint& start, int rows,
                                           int columns) {
  QList<QRectF> ret;
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      auto row = start.x() + i;
      auto col = start.y() + j;
      if (baseGrid.count() <= row) continue;
      if (baseGrid.at(row).count() <= col) continue;
      ret << baseGrid[row][col].first;
    }
  }
  return ret;
}

void LayoutCalculatorV1::setGridOccupied(GridTable& baseGrid,
                                         const QPoint& start, int rows,
                                         int columns, QLayoutItem* item) {
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      auto row = start.x() + i;
      auto col = start.y() + j;
      if (baseGrid.count() <= row) continue;
      if (baseGrid.at(row).count() <= col) continue;
      baseGrid[row][col].second = item;
    }
  }
}

void LayoutCalculatorV1::resetGridOccupied(
    GridTable& baseGrid, const QList<QLayoutItem*>& keepList) {
  for (auto& row : baseGrid) {
    for (auto& pr : row) {
      if (!keepList.contains(pr.second)) {
        pr = qMakePair(pr.first, nullptr);
      }
    }
  }
}

QLayoutItem* LayoutCalculatorV1::layoutedItemAt(const GridTable& baseGrid,
                                                const QPoint& pos) {
  for (const auto& rows : baseGrid) {
    for (const auto& pr : rows) {
      if (pr.first.contains(pos)) return pr.second;
    }
  }
  return nullptr;
}

QLayoutItem* LayoutCalculatorV1::layoutedItemBeforePos(
    const GridTable& baseGrid, const QPoint& pos) {
  Q_UNUSED(pos)
  Q_UNUSED(baseGrid)
  return nullptr;
}

QList<QRectF> LayoutCalculatorV1::baseGridRects() const {
  QList<QRectF> result;
  for (const auto& vec : base_grid_) {
    for (const auto& pr : vec) {
      result << pr.first;
    }
  }
  return result;
}

QList<QRectF> LayoutCalculatorV1::dargJudgeGridRects() const {
  return {};
}

QList<QPair<QRectF, bool>> LayoutCalculatorV1::layoutedGridRects() const {
  QList<QPair<QRectF, bool>> result;
  for (const auto& vec : base_grid_) {
    for (const auto& p : vec) {
      result << p;
    }
  }
  return result;
}

}  // namespace etest::app::grid
