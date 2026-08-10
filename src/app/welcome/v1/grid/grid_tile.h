#ifndef ETEST_APP_GRID_GRID_TILE_H_
#define ETEST_APP_GRID_GRID_TILE_H_

#include <QLinearGradient>
#include <QRectF>
#include <QWidget>

#include "grid_global_def.hpp"

class QLabel;
class QPropertyAnimation;
class QVBoxLayout;

namespace etest::app::grid {

class GridTile : public QWidget {
  Q_OBJECT
 public:
  GridTile(TileSpan type, QWidget* parent = nullptr);

  int type() const { return type_; }

  void setContentWidget(QWidget* widget);
  QWidget* contentWidget() const { return content_widget_; }
  void setNameText(const QString& name);
  QString nameText() const;

  void setDragingState(bool draging);

  void posResetAnimation(const QRect& boundary, const QPoint& pos1,
                         const QPoint& pos2);
  QPoint dragUsedPos() const;

 signals:
  void clicked();
  void contextMenuRequested(const QPoint& globalPos, int type);

 protected:
  bool event(QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  void initUi();
  void calcFixedSize();
  void doMouseHoverEnterLeave(bool enter);

  bool draging_ = false;
  QVBoxLayout* layout_ = nullptr;
  QWidget* content_widget_ = nullptr;
  QLabel* name_label_ = nullptr;
  TileSpan type_ = _1_1;
  int gradient_index_ = 0;

  QPropertyAnimation* shake_anime_ = nullptr;
  QPropertyAnimation* pos_anime_ = nullptr;

  // 渐变缓存
  QLinearGradient cached_gradient_;
  QRectF cached_gradient_rect_;
};

}  // namespace etest::app::grid

#endif  // ETEST_APP_GRID_GRID_TILE_H_
