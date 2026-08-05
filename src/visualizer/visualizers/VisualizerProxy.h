#pragma once

#include <QGraphicsProxyWidget>
#include <QPointF>
#include <QSizeF>
#include <QVariant>

namespace etest::visualizer {

// VisualizerProxy — 包裹 visualizer 卡片的 QGraphicsProxyWidget 子类。
// 内嵌拓扑 TopologyBlockItem 的移动 + 8 方向 resize 手柄逻辑，供可视化区
// 编辑态使用；展示态（edit_mode_=false）只读，不可移动、不显示手柄。
// 复用点（拓扑 TopologyBlockItem）：resizeHandleRect/handleAt/updateCursorForHandle/
// mousePress/Move/Release 状态机 + doResize；差异在落点改为 widget()->resize。
class VisualizerProxy : public QGraphicsProxyWidget {
  Q_OBJECT

 public:
  enum { Type = UserType + 50 };
  int type() const override { return Type; }

  explicit VisualizerProxy(QGraphicsItem* parent = nullptr);

  // 编辑/展示两态：展示态禁移动、不画手柄
  void setEditMode(bool edit);
  bool editMode() const { return edit_mode_; }

  // 实际 widget 矩形（scene 坐标，不含 resize 手柄外扩边距）
  QRectF visualRect() const;

 signals:
  // 用户交互（拖动/拖拽 resize）会话结束且几何实际变化时发出，供宿主置脏
  void geometryEdited();

 protected:
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

 private:
  enum class ResizeHandle {
    None = 0,
    TopLeft,
    TopMid,
    TopRight,
    RightMid,
    BottomRight,
    BottomMid,
    BottomLeft,
    LeftMid,
  };

  QRectF handleRect(ResizeHandle h) const;
  ResizeHandle handleAt(const QPointF& pos) const;
  void updateCursorForHandle(ResizeHandle h);
  void doResize(const QPointF& delta);

  bool edit_mode_ = true;
  bool drag_active_ = false;  // 拖动会话中（press 起 / release 止）
  bool moved_ = false;        // 本会话内发生过移动
  bool resized_ = false;      // 本会话内发生过 resize
  ResizeHandle active_handle_ = ResizeHandle::None;
  QPointF resize_start_pos_;

  static constexpr qreal kHandleSize = 8.0;
  static constexpr qreal kHandleMargin = 6.0;  // boundingRect 外扩边距，包住手柄
};

}  // namespace etest::visualizer
