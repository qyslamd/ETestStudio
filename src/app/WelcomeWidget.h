#ifndef ETEST_APP_WELCOME_WIDGET_H_
#define ETEST_APP_WELCOME_WIDGET_H_

#include <QLabel>
#include <QPixmap>
#include <QStringList>
#include <QWidget>

#include "widgets/EyeWidget.h"

namespace etest::app::grid {
class GridLayout;
class GridTile;
}  // namespace etest::app::grid

namespace etest::app {

class WelcomeWidget : public QWidget {
  Q_OBJECT

 public:
  enum DragPreviewStyle { None = 0, Grid, ShadowImg, PureColor };

  explicit WelcomeWidget(QWidget* parent = nullptr);

  void refreshRecentProjects();
  void loadBackground();

  // 网格叠加层
  void setGridOverlayVisible(bool visible);
  bool gridOverlayVisible() const;
  void setGridColor(const QColor& c);
  void setOccupiedGridColor(const QColor& c);

  // 拖拽预览
  void setDragPreviewStyle(DragPreviewStyle style);
  DragPreviewStyle dragPreviewStyle() const;

 signals:
  void newProjectRequested();
  void openProjectRequested();
  void projectOpenRequested(const QString& projectPath);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  bool eventFilter(QObject* obj, QEvent* event) override;

  // 拖拽重排
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void rebuildRecentTiles();
  void showRandomTip();
  grid::GridTile* getTileUnderMouse(QWidget* child) const;

  grid::GridLayout* grid_layout_ = nullptr;
  grid::GridTile* tip_tile_ = nullptr;
  QLabel* tip_content_label_ = nullptr;
  EyeWidget* eye_widget_ = nullptr;

  QList<grid::GridTile*> recent_tiles_;

  // 拖拽重排
  QPoint drag_start_pos_;
  bool enable_drag_edit_ = true;
  QList<QRectF> best_drop_rect_;
  QPixmap drop_pixmap_;

  // 背景图片
  QPixmap bg_pixmap_;
  QString bg_image_path_;
  QString bg_dir_path_;
  int bg_mode_ = 0;
  QStringList image_filters_{
      "*.png", "*.jpg", "*.jpeg", "*.jfif", "*.bmp", "*.gif", "*.svg"};

  // 每日提示
  QStringList tips_;

  // 网格叠加层
  bool draw_grid_overlay_ = true;
  QColor grid_color_{180, 180, 180, 80};
  QColor occupied_grid_color_{100, 180, 100, 60};

  // 拖拽预览样式
  DragPreviewStyle drag_preview_style_ = PureColor;
};

}  // namespace etest::app

#endif  // ETEST_APP_WELCOME_WIDGET_H_
