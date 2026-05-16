#pragma once

#include <QGraphicsScene>
#include <QVector>

class QGraphicsLineItem;

namespace topology {

class TopologyDocument;
class UutItem;
class DeviceItem;
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

    // Connection drag interaction (called from PortItem)
    void startConnectionDrag(PortItem* port, QPointF scenePos);
    void continueConnectionDrag(QPointF scenePos);
    void finishConnectionDrag(QPointF scenePos);

    // Called when an item moves (from itemChange)
    void onItemMoved();

    // Find items at scene position by type
    DeviceItem* deviceItemAt(QPointF scenePos) const;
    UutItem* uutItemAt(QPointF scenePos) const;

signals:
    void itemSelected(QGraphicsItem* item);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void clearScene();

    TopologyDocument* doc_;
    QVector<UutItem*> uut_items_;
    QVector<DeviceItem*> device_items_;
    QVector<ConnectionItem*> connection_items_;

    // Drag state
    PortItem* drag_source_ = nullptr;
    QGraphicsLineItem* drag_line_ = nullptr;
};

}  // namespace topology
