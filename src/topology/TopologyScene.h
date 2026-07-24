#pragma once

#include <QGraphicsScene>
#include <QGraphicsSceneDragDropEvent>
#include <QJsonObject>
#include <QVector>

class QGraphicsLineItem;

namespace etest::topology {

class TopologyDocument;
class UutItem;
class DeviceItem;
class DevicePortItem;
class UutPortItem;
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

  // Connection drag interaction (called from UutPortItem / DevicePortItem)
  void startConnectionDrag(QGraphicsItem* port, QPointF scenePos);
  void continueConnectionDrag(QPointF scenePos);
  void finishConnectionDrag(QPointF scenePos);

  // Called when an item moves (from itemChange)
  void onItemMoved();

  // Find items by index
  UutItem* findUutItem(int productIndex) const;
  DeviceItem* findDeviceItem(int deviceIndex) const;

  // Find items by index
  ConnectionItem* findConnectionItem(int connIndex) const;

  // Find items at scene position by type
  DeviceItem* deviceItemAt(QPointF scenePos) const;
  DevicePortItem* devicePortItemAt(QPointF scenePos) const;
  UutPortItem* portItemAt(QPointF scenePos) const;
  UutItem* uutItemAt(QPointF scenePos) const;
  ConnectionItem* connectionItemAt(QPointF scenePos) const;

  /// Called from ConnectionItem when badge is clicked
  void emitMonitorBadgeClicked(int connIdx, int monIdx);

 signals:
  void itemSelected(QGraphicsItem* item);
  void deviceDropped(const QString& deviceType,
                     int channelCount,
                     int direction,
                     int functionType,
                     const QString& pluginId,
                     const QPointF& scenePos);
  /// Emitted when a connection badge is clicked
  void monitorBadgeClicked(int connIdx, int monIdx);

 public:
  // Refresh connection badge indicators
  void updateMonitorBadges();

 protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

 private:
  void clearScene();
  void createDragPreview();

  TopologyDocument* doc_;
  QVector<UutItem*> uut_items_;
  QVector<DeviceItem*> device_items_;
  QVector<ConnectionItem*> connection_items_;

  // Connection drag state (UutPortItem or DevicePortItem)
  QGraphicsItem* drag_source_ = nullptr;
  QGraphicsLineItem* drag_line_ = nullptr;

  // Device palette drag preview
  QGraphicsItem* drag_preview_ = nullptr;
  QJsonObject drag_preview_data_;

  // Move tracking
  QGraphicsItem* moving_item_ = nullptr;
  QPointF move_start_pos_;

  // Flag to suppress itemSelected emission when badge click was handled
  bool badge_click_handled_ = false;
};

}  // namespace etest::topology
