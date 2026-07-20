#ifndef ETEST_APP_GRID_LAYOUT_CALCULATOR_V3_H_
#define ETEST_APP_GRID_LAYOUT_CALCULATOR_V3_H_

#include <QMap>
#include <QVector>

#include "layout_calculator_base.h"
#include "grid_global_def.hpp"

namespace etest::app::grid {

class LayoutCalculatorV3 : public ILayoutCalculator {
 public:
  LayoutCalculatorV3(GridLayout* p);
  ~LayoutCalculatorV3() override;

  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int) const override { return -1; }

  void calcBaseGridRects(const QRect& rect) override;
  QList<QRectF> baseGridRects() const override;
  QList<QRectF> dargJudgeGridRects() const override;
  QList<QPair<QRectF, bool>> layoutedGridRects() const override;
  void doLayout(const QRect& rect) override;
  QList<QRectF> dealWithDragMove(GridTile* widget, const QPoint& pos,
                                 DragTrend trend) override;
  void dropApplied(bool ok) override;

 protected:
  QRectF rectAtPoint(const QPoint& pos) const override;
  QRectF rectAt(int row, int column) const override;
  void indexOfRect(const QRectF& rect, int& row, int& column) const override;

 private:
  using GridTable = QVector<QVector<QPair<QRectF, QLayoutItem*>>>;

  GridTable base_grid_;
  QVector<QVector<QRectF>> drag_use_grid_;

  void indexOfDragRect(const QPoint& pos, int& row, int& col) const;
  bool isNextGridCanLayout(const QPoint& startPos, int row_span,
                           int col_span) const;
  QPoint nextCanLayoutPos(int row_span, int column_span) const;
  QPoint findDragMoveOkPos(const QPoint& pos, int row_span,
                           int col_span) const;
  void setGridOccupied(const QPoint& start, int rows, int columns,
                       QLayoutItem* item);

  struct PosInfo {
    QPoint start;
    int row_span;
    int col_span;

    bool valid() const {
      return start.x() >= 0 && start.y() >= 0 && row_span > 0 && col_span > 0;
    }

    friend bool operator==(const PosInfo& r1, const PosInfo& r2) {
      return r1.start == r2.start && r1.row_span == r2.row_span &&
             r1.col_span == r2.col_span;
    }
  };

  QMap<QLayoutItem*, PosInfo> layout_map_;
  std::tuple<QLayoutItem*, PosInfo, PosInfo> drag_cached_;
  QList<QRectF> posInfoRects(const PosInfo& info) const;
};

}  // namespace etest::app::grid

#endif  // ETEST_APP_GRID_LAYOUT_CALCULATOR_V3_H_
