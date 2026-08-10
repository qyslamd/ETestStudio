#ifndef ETEST_APP_GRID_GRID_LAYOUT_H_
#define ETEST_APP_GRID_GRID_LAYOUT_H_

#include <QLayout>

#include "grid_global_def.hpp"

namespace etest::app::grid {

class GridTile;
class ILayoutCalculator;

class GridLayout : public QLayout {
  Q_OBJECT
 public:
  explicit GridLayout(QWidget* parent);
  ~GridLayout() override;

  void setAutoLayout(bool enable);

 signals:
  void baseGridChanged();

 public:
  void addItem(QLayoutItem* item) override;
  Qt::Orientations expandingDirections() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(int) const override;
  int count() const override;
  QLayoutItem* itemAt(int index) const override;
  QSize minimumSize() const override;
  void setGeometry(const QRect& rect) override;
  QSize sizeHint() const override;
  QLayoutItem* takeAt(int index) override;

  QList<QRectF> baseGrid() const;
  QList<QRectF> dragGrids() const;
  QList<QPair<QRectF, bool>> layoutedGrid() const;
  QRectF layoutArea() const;

  QList<QRectF> dealWithDragMove(GridTile* item, const QPoint& pos,
                                 DragTrend trend);
  void dropApplied(bool ok);

 private:
  ILayoutCalculator* d_ = nullptr;
};

}  // namespace etest::app::grid

#endif  // ETEST_APP_GRID_GRID_LAYOUT_H_
