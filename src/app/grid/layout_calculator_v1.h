#ifndef ETEST_APP_GRID_LAYOUT_CALCULATOR_V1_H_
#define ETEST_APP_GRID_LAYOUT_CALCULATOR_V1_H_

#include "layout_calculator_base.h"

#include <QVector>

namespace etest::app::grid {

class LayoutCalculatorV1 : public ILayoutCalculator {
 public:
  explicit LayoutCalculatorV1(GridLayout* p);
  ~LayoutCalculatorV1() override;

  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int) const override;

  void calcBaseGridRects(const QRect& rect) override;
  QList<QRectF> baseGridRects() const override;
  QList<QRectF> dargJudgeGridRects() const override;
  QList<QPair<QRectF, bool>> layoutedGridRects() const override;
  void doLayout(const QRect& rect) override;
  QList<QRectF> dealWithDragMove(GridTile* item, const QPoint& pos,
                                 DragTrend trend) override;
  void dropApplied(bool ok) override;

 protected:
  QRectF rectAtPoint(const QPoint& pos) const override;
  QRectF rectAt(int row, int column) const override;
  void indexOfRect(const QRectF& rect, int& row, int& column) const override;

 private:
  using GridTable = QVector<QVector<QPair<QRectF, QLayoutItem*>>>;

  static bool isNextGridsAvailable(const GridTable& baseGrid,
                                   const QPoint& posStart, int row_span,
                                   int col_span);
  static QPoint nextAvailableGridPos(const GridTable& baseGrid, int row_span,
                                     int column_span);
  static QList<QRectF> getRects(const GridTable& baseGrid,
                                const QPoint& start, int rows, int columns);
  static void setGridOccupied(GridTable& baseGrid, const QPoint& start,
                              int rows, int columns, QLayoutItem* item);
  static void resetGridOccupied(GridTable& baseGrid,
                                const QList<QLayoutItem*>& keepList);
  static QLayoutItem* layoutedItemAt(const GridTable& baseGrid,
                                     const QPoint& pos);
  static QLayoutItem* layoutedItemBeforePos(const GridTable& baseGrid,
                                            const QPoint& pos);

  GridTable base_grid_;
  QList<QLayoutItem*> drag_cached_item_list_;
  GridTable drag_cached_base_grid_;
};

}  // namespace etest::app::grid

#endif  // ETEST_APP_GRID_LAYOUT_CALCULATOR_V1_H_
