#ifndef ETEST_APP_GRID_LAYOUT_CALCULATOR_BASE_H_
#define ETEST_APP_GRID_LAYOUT_CALCULATOR_BASE_H_

#include <QList>
#include <QPair>
#include <QRectF>
#include <QVariant>

#include "grid_global_def.hpp"

class QLayoutItem;
class QWidget;

namespace etest::app::grid {

class GridTile;
class GridAnimator;
class GridLayout;

class ILayoutCalculator {
 public:
  ILayoutCalculator(GridLayout* p);
  virtual ~ILayoutCalculator();

  QList<QLayoutItem*> item_list_;
  QRectF layoutArea() const { return layout_area_; }

  static int calcLineGridCount(double totalLength, double unitLength,
                               int spacing, double& halfUnused);

  virtual bool hasHeightForWidth() const = 0;
  virtual int heightForWidth(int) const = 0;

  virtual void calcBaseGridRects(const QRect& rect) = 0;
  virtual QList<QRectF> baseGridRects() const = 0;
  virtual QList<QRectF> dargJudgeGridRects() const = 0;
  virtual QList<QPair<QRectF, bool>> layoutedGridRects() const = 0;
  virtual void doLayout(const QRect& rect) = 0;

  virtual QList<QRectF> dealWithDragMove(GridTile* item,
                                         const QPoint& pos,
                                         DragTrend trend) = 0;
  virtual void dropApplied(bool ok) = 0;

 protected:
  static QLayoutItem* getLayoutIem(const QList<QLayoutItem*>& itemList,
                                   QWidget* widget);
  virtual QRectF rectAt(int row, int column) const = 0;
  virtual void indexOfRect(const QRectF& rect, int& row, int& column) const = 0;
  virtual QRectF rectAtPoint(const QPoint& pos) const;
  void calcGridsNeeded(QLayoutItem* item, int& rows, int& columns) const;

  GridLayout* p_ = nullptr;
  GridAnimator* animator_ = nullptr;
  QRectF layout_area_;
};

}  // namespace etest::app::grid

#endif  // ETEST_APP_GRID_LAYOUT_CALCULATOR_BASE_H_
