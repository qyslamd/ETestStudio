#include "layout_calculator_base.h"

#include <QRandomGenerator>
#include <QtGlobal>
#include <QtMath>

#include "grid_animator.h"
#include "grid_tile.h"
#include "grid_layout.h"

namespace etest::app::grid {

ILayoutCalculator::ILayoutCalculator(GridLayout* p)
    : p_(p), animator_(new GridAnimator(p_)) {}

ILayoutCalculator::~ILayoutCalculator() {
  QLayoutItem* item;
  while ((item = p_->takeAt(0))) delete item;
}

int ILayoutCalculator::calcLineGridCount(double totalLength, double unitLength,
                                         int spacing, double& halfUnused) {
  if (unitLength > totalLength) {
    halfUnused = 0.0;
    return 0;
  }
  int count = 1;
  double calcLength = 0;
  while (true) {
    calcLength = count * unitLength + (count - 1) * spacing;
    if (calcLength > totalLength) {
      count--;
      break;
    } else if (qFuzzyCompare(calcLength, totalLength)) {
      break;
    } else {
      count++;
    }
  }
  halfUnused =
      (totalLength - (count * unitLength + (count - 1) * spacing)) / 2;
  if (halfUnused < 0) halfUnused = 0;
  return count;
}

QLayoutItem* ILayoutCalculator::getLayoutIem(
    const QList<QLayoutItem*>& itemList, QWidget* widget) {
  for (auto item : itemList) {
    if (item->widget() == widget) return item;
  }
  return nullptr;
}

void ILayoutCalculator::calcGridsNeeded(QLayoutItem* item, int& rows,
                                        int& columns) const {
  auto label = qobject_cast<GridTile*>(item->widget());
  if (!label) {
    rows = 0;
    columns = 0;
    return;
  }
  auto list = decima2HexStringList(label->type());
  if (list.count() != 2) {
    rows = 0;
    columns = 0;
    return;
  }
  rows = list.at(0).toInt();
  columns = list.at(1).toInt();
}

QRectF ILayoutCalculator::rectAtPoint(const QPoint& pos) const {
  auto baseGrid = baseGridRects();
  for (auto it = baseGrid.begin(); it != baseGrid.end(); ++it) {
    if ((*it).contains(pos)) return *it;
  }
  return QRectF();
}

}  // namespace etest::app::grid
