#include "TopologyScene.h"
#include "TopologyDocument.h"
#include "topology_items.h"

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsLineItem>
#include <QPen>

namespace topology {

TopologyScene::TopologyScene(TopologyDocument* doc, QObject* parent)
    : QGraphicsScene(parent), doc_(doc) {}

void TopologyScene::loadFromDocument() {
    clearScene();

    for (int i = 0; i < doc_->productCount(); ++i) {
        const auto* prod = doc_->product(i);
        addProductItem(i, prod->position);
    }
    for (int i = 0; i < doc_->deviceCount(); ++i) {
        const auto* dev = doc_->device(i);
        addDeviceItem(i, dev->position);
    }
    for (int i = 0; i < doc_->connectionCount(); ++i) {
        addConnectionItem(i);
    }
}

void TopologyScene::syncPositionsToDocument() {
    for (auto* uut : uut_items_) {
        if (auto* prod = doc_->product(uut->productIndex())) {
            prod->position = uut->pos();
        }
    }
    for (auto* dev : device_items_) {
        if (auto* d = doc_->device(dev->deviceIndex())) {
            d->position = dev->pos();
        }
    }
}

UutItem* TopologyScene::addProductItem(int productIndex, const QPointF& pos) {
    auto* item = new UutItem(productIndex, doc_);
    item->setPos(pos);
    addItem(item);
    uut_items_.append(item);
    return item;
}

DeviceItem* TopologyScene::addDeviceItem(int deviceIndex, const QPointF& pos) {
    auto* item = new DeviceItem(deviceIndex, doc_);
    item->setPos(pos);
    addItem(item);
    device_items_.append(item);
    return item;
}

ConnectionItem* TopologyScene::addConnectionItem(int connIndex) {
    const auto* conn = doc_->connection(connIndex);
    if (!conn) return nullptr;

    PortItem* sourcePort = nullptr;
    DevicePortItem* targetPort = nullptr;

    // Find source port
    for (auto* uut : uut_items_) {
        const auto* prod = doc_->product(uut->productIndex());
        if (prod && prod->name == conn->productName) {
            for (int pi = 0; pi < prod->ports.size(); ++pi) {
                if (prod->ports[pi].name == conn->portName) {
                    sourcePort = uut->portItem(pi);
                    break;
                }
            }
            break;
        }
    }

    // Find target device port
    for (auto* dev : device_items_) {
        const auto* d = doc_->device(dev->deviceIndex());
        if (d && d->name == conn->deviceName) {
            for (int pi = 0; pi < d->ports.size(); ++pi) {
                if (d->ports[pi].name == conn->devicePort) {
                    targetPort = dev->devicePortItem(pi);
                    break;
                }
            }
            break;
        }
    }

    if (!sourcePort || !targetPort) return nullptr;

    auto* item = new ConnectionItem(sourcePort, targetPort, conn->devicePort);
    addItem(item);
    item->updatePath();
    connection_items_.append(item);
    return item;
}

void TopologyScene::startConnectionDrag(PortItem* port, QPointF scenePos) {
    if (drag_source_) return;
    drag_source_ = port;
    drag_line_ = new QGraphicsLineItem();
    drag_line_->setPen(QPen(QColor(100, 100, 100), 2, Qt::DashLine));
    drag_line_->setZValue(10);
    addItem(drag_line_);
    drag_line_->setLine(QLineF(scenePos, scenePos));
}

void TopologyScene::continueConnectionDrag(QPointF scenePos) {
    if (drag_line_) {
        QLineF line(drag_source_->sceneCenter(), scenePos);
        drag_line_->setLine(line);
    }
}

void TopologyScene::finishConnectionDrag(QPointF scenePos) {
    if (!drag_source_) return;

    // Clean up drag line
    if (drag_line_) {
        removeItem(drag_line_);
        delete drag_line_;
        drag_line_ = nullptr;
    }

    // Hit test for DevicePortItem
    auto* devPort = devicePortItemAt(scenePos);
    if (devPort) {
        const auto* prod = doc_->product(drag_source_->productIndex());
        const auto* dev = doc_->device(devPort->deviceIndex());
        if (prod && dev && devPort->portIndex() < dev->ports.size()) {
            const auto& port = prod->ports[drag_source_->portIndex()];
            const auto& dp = dev->ports[devPort->portIndex()];
            if (doc_->canConnect(prod->name, port.name, dev->name, dp.name)) {
                TopologyConnection conn{prod->name, port.name, dev->name, dp.name};
                int ci = doc_->addConnection(conn);
                addConnectionItem(ci);
            }
        }
    }

    drag_source_ = nullptr;
}

void TopologyScene::onItemMoved() {
    for (auto* conn : connection_items_) {
        if (conn) conn->updatePath();
    }
}

DeviceItem* TopologyScene::deviceItemAt(QPointF scenePos) const {
    auto items = this->items(scenePos, Qt::IntersectsItemBoundingRect,
                             Qt::DescendingOrder);
    for (auto* item : items) {
        if (auto* dev = qgraphicsitem_cast<DeviceItem*>(item)) {
            return dev;
        }
        // Check children
        if (auto* dev = item->parentItem()
                ? qgraphicsitem_cast<DeviceItem*>(item->parentItem())
                : nullptr) {
            return dev;
        }
    }
    return nullptr;
}

DevicePortItem* TopologyScene::devicePortItemAt(QPointF scenePos) const {
    auto items = this->items(scenePos, Qt::IntersectsItemBoundingRect,
                             Qt::DescendingOrder);
    for (auto* item : items) {
        if (auto* dp = qgraphicsitem_cast<DevicePortItem*>(item)) {
            return dp;
        }
    }
    return nullptr;
}

UutItem* TopologyScene::uutItemAt(QPointF scenePos) const {
    auto items = this->items(scenePos, Qt::IntersectsItemBoundingRect,
                             Qt::DescendingOrder);
    for (auto* item : items) {
        if (auto* uut = qgraphicsitem_cast<UutItem*>(item)) {
            return uut;
        }
    }
    return nullptr;
}

void TopologyScene::clearScene() {
    uut_items_.clear();
    device_items_.clear();
    connection_items_.clear();
    clear();
}

void TopologyScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsScene::mousePressEvent(event);

    if (event->button() == Qt::LeftButton) {
        // Notify property panel of selection change
        auto selected = selectedItems();
        if (!selected.isEmpty()) {
            emit itemSelected(selected.first());
        } else {
            emit itemSelected(nullptr);
        }
    }
}

}  // namespace topology
