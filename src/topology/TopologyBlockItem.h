#pragma once

#include <QGraphicsItem>
#include <QPen>
#include <QRectF>
#include <QVector>

QT_BEGIN_NAMESPACE
class QColor;
class QPainter;
class QGraphicsSceneMouseEvent;
class QStyleOptionGraphicsItem;
QT_END_NAMESPACE

namespace etest::topology {

class TopologyDocument;

/// Common base for UutItem and DeviceItem.
/// Handles hover/shadow/select effects, shape/hit-test, move tracking,
/// and 8-direction resize handles (drawn when selected).
class TopologyBlockItem : public QGraphicsItem {
 public:
  enum class ResizeHandle { None, TopLeft, TopMid, TopRight,
                            RightMid, BottomRight, BottomMid, BottomLeft, LeftMid };

  TopologyBlockItem(TopologyDocument* doc,
                    qreal width,
                    qreal cornerRadius,
                    QGraphicsItem* parent = nullptr);

  QRectF boundingRect() const override final;
  QPainterPath shape() const override final;
  bool contains(const QPointF& point) const override final;
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget) override final;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) override final;

  TopologyDocument* document() const { return doc_; }

 protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override final;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override final;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override final;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override final;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override final;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override final;

  // Subclass hooks — colors, content, layout
  virtual QColor blockFillColor() const = 0;
  virtual QColor blockBorderColor() const = 0;
  // Override to change border pen style (e.g. dashed for monitors)
  virtual QPen blockBorderPen(qreal penWidth) const {
    return QPen(blockBorderColor(), penWidth);
  }
  virtual void paintContent(QPainter* painter,
                            const QStyleOptionGraphicsItem* option,
                            const QRectF& rect) = 0;
  virtual qreal calcContentHeight() const = 0;
  // Override to suppress body darkening when a child port is hovered
  virtual bool hasChildHovered() const { return false; }
  // Called after a resize drag completes — subclass pushes UndoCommand + updates doc
  virtual void onResizeFinished(const QSizeF& oldSize, const QPointF& oldPos) {}

  // Register child ports for accurate contains() hit-test
  void addChildPort(QGraphicsItem* port);
  void clearChildPorts();

  // Effective height: user-set override or auto-computed
  qreal effectiveHeight() const {
    return block_height_ > 0 ? block_height_ : calcContentHeight();
  }

  TopologyDocument* doc_ = nullptr;
  qreal block_width_ = 120.0;
  qreal corner_radius_ = 8.0;
  qreal block_height_ = 0;   // 0 = auto (uses calcContentHeight())
  bool body_hovered_ = false;

 private:
  bool isOverChildPort(const QPointF& point) const;
  QVector<QGraphicsItem*> child_ports_;

  // Resize support
  ResizeHandle handleAt(const QPointF& pos) const;
  QRectF resizeHandleRect(ResizeHandle h) const;
  void updateCursorForHandle(ResizeHandle h);
  void doResize(const QPointF& delta);

  ResizeHandle active_handle_ = ResizeHandle::None;
  QPointF resize_start_pos_;
  QPointF old_pos_on_press_;
  qreal old_width_on_press_ = 0;
  qreal old_height_on_press_ = 0;
};

}  // namespace etest::topology
