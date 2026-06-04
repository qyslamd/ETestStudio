#include "grid_layout.h"

#include <QWidget>

#include "layout_calculator_base.h"
#include "layout_calculator_v1.h"
#include "layout_calculator_v3.h"

namespace etest::app::grid {

GridLayout::GridLayout(QWidget* parent)
    : QLayout(parent), d_(new LayoutCalculatorV3(this)) {}

GridLayout::~GridLayout() { delete d_; }

void GridLayout::setAutoLayout(bool enable) {
  // enable=true → V1（每次从左上角自动排列）
  // enable=false → V3（记住拖拽后的位置）
  auto old_d = d_;
  if (enable) {
    d_ = new LayoutCalculatorV1(this);
  } else {
    d_ = new LayoutCalculatorV3(this);
  }
  if (old_d) {
    d_->item_list_ = old_d->item_list_;
    delete old_d;
  }
  update();
}

void GridLayout::addItem(QLayoutItem* item) {
  d_->item_list_.append(item);
}

int GridLayout::count() const { return d_->item_list_.size(); }

QLayoutItem* GridLayout::itemAt(int index) const {
  return d_->item_list_.value(index);
}

QLayoutItem* GridLayout::takeAt(int index) {
  if (index >= 0 && index < d_->item_list_.size())
    return d_->item_list_.takeAt(index);
  return nullptr;
}

QList<QRectF> GridLayout::dealWithDragMove(GridTile* item,
                                           const QPoint& pos,
                                           DragTrend trend) {
  return d_->dealWithDragMove(item, pos, trend);
}

void GridLayout::dropApplied(bool ok) { d_->dropApplied(ok); }

QList<QRectF> GridLayout::baseGrid() const {
  return d_->baseGridRects();
}

QList<QRectF> GridLayout::dragGrids() const {
  return d_->dargJudgeGridRects();
}

QList<QPair<QRectF, bool>> GridLayout::layoutedGrid() const {
  return d_->layoutedGridRects();
}

QRectF GridLayout::layoutArea() const { return d_->layoutArea(); }

Qt::Orientations GridLayout::expandingDirections() const { return {}; }

bool GridLayout::hasHeightForWidth() const {
  return d_->hasHeightForWidth();
}

int GridLayout::heightForWidth(int width) const {
  return d_->heightForWidth(width);
}

void GridLayout::setGeometry(const QRect& rect) {
  QLayout::setGeometry(rect);
  d_->calcBaseGridRects(rect);
  emit baseGridChanged();
  d_->doLayout(rect);
}

QSize GridLayout::sizeHint() const { return minimumSize(); }

QSize GridLayout::minimumSize() const {
  QSize size;
  for (auto item : d_->item_list_)
    size = size.expandedTo(item->minimumSize());
  size += QSize(2 * margin(), 2 * margin());
  return size;
}

}  // namespace etest::app::grid
