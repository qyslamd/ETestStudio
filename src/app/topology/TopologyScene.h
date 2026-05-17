#pragma once

#include <QGraphicsScene>
#include <QVector>

class QGraphicsLineItem;

namespace etest::topology {

class TopologyDocument;
class UutItem;
class DeviceItem;
class DevicePortItem;
class PortItem;
class ConnectionItem;

class TopologyScene : public QGraphicsScene {
  Q_OBJECT
 public:
  explicit TopologyScene(TopologyDocument* doc, QObject* parent = nullptr);

  TopologyDocument* document() const { return doc_; }

  // Build scene from document
  void loadFromDocument();
  // Sync document positions back from graphics items
  void syncPositionsToDocument();

  // Add elements
  UutItem* addProductItem(int productIndex, const QPointF& pos);
  DeviceItem* addDeviceItem(int deviceIndex, const QPointF& pos);
  ConnectionItem* addConnectionItem(int connIndex);

  // Connection drag interaction (called from PortItem / DevicePortItem)
  void startConnectionDrag(QGraphicsItem* port, QPointF scenePos);
  void continueConnectionDrag(QPointF scenePos);
  void finishConnectionDrag(QPointF scenePos);

  // Called when an item moves (from itemChange)
  void onItemMoved();

  // Find items by index
  UutItem* findUutItem(int productIndex) const;
  DeviceItem* findDeviceItem(int deviceIndex) const;

  // Find items at scene position by type
  DeviceItem* deviceItemAt(QPointF scenePos) const;
  DevicePortItem* devicePortItemAt(QPointF scenePos) const;
  PortItem* portItemAt(QPointF scenePos) const;
  UutItem* uutItemAt(QPointF scenePos) const;
  ConnectionItem* connectionItemAt(QPointF scenePos) const;

 signals:
  void itemSelected(QGraphicsItem* item);

 protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

 private:
  void clearScene();

  TopologyDocument* doc_;
  QVector<UutItem*> uut_items_;
  QVector<DeviceItem*> device_items_;
  QVector<ConnectionItem*> connection_items_;

  // Drag state (PortItem or DevicePortItem)
  QGraphicsItem* drag_source_ = nullptr;
  QGraphicsLineItem* drag_line_ = nullptr;

  // Move tracking
  QGraphicsItem* moving_item_ = nullptr;
  QPointF move_start_pos_;
};

}  // namespace etest::topology
