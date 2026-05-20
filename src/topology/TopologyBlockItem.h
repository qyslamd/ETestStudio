#pragma once

#include <QGraphicsItem>
#include <QRectF>
#include <QVector>

QT_BEGIN_NAMESPACE
class QColor;
class QPainter;
class QStyleOptionGraphicsItem;
QT_END_NAMESPACE

namespace etest::topology {

class TopologyDocument;

/// Common base for UutItem and DeviceItem.
/// Handles hover/shadow/select effects, shape/hit-test, and move tracking.
/// Subclasses implement paintContent(), calcContentHeight(), and color hooks.
class TopologyBlockItem : public QGraphicsItem {
 public:
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

  // Subclass hooks — colors, content, layout
  virtual QColor blockFillColor() const = 0;
  virtual QColor blockBorderColor() const = 0;
  virtual void paintContent(QPainter* painter,
                            const QStyleOptionGraphicsItem* option,
                            const QRectF& rect) = 0;
  virtual qreal calcContentHeight() const = 0;
  // Override to suppress body darkening when a child port is hovered
  virtual bool hasChildHovered() const { return false; }

  // Register child ports for accurate contains() hit-test
  void addChildPort(QGraphicsItem* port);
  void clearChildPorts();

  TopologyDocument* doc_ = nullptr;
  qreal block_width_ = 120.0;
  qreal corner_radius_ = 8.0;
  bool body_hovered_ = false;

 private:
  bool isOverChildPort(const QPointF& point) const;
  QVector<QGraphicsItem*> child_ports_;
};

}  // namespace etest::topology
