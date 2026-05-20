#include "TopologyScene.h"
#include "TopologyDocument.h"
#include "TopologyTheme.h"
#include "UndoCommands.h"
#include "topology_items.h"

#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QPen>

namespace etest::topology {

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
  if (!conn)
    return nullptr;

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

  if (!sourcePort || !targetPort)
    return nullptr;

  auto* item =
      new ConnectionItem(sourcePort, targetPort, conn->devicePort, doc_);
  item->setConnectionIndex(connIndex);
  addItem(item);
  item->setStyle(conn->style);
  item->updatePath();
  connection_items_.append(item);
  return item;
}

void TopologyScene::startConnectionDrag(QGraphicsItem* port, QPointF scenePos) {
  if (drag_source_)
    return;
  drag_source_ = port;
  drag_line_ = new QGraphicsLineItem();
  drag_line_->setPen(QPen(topologyColors().connectionLine, 2, Qt::DashLine));
  drag_line_->setZValue(10);
  addItem(drag_line_);
  drag_line_->setLine(QLineF(scenePos, scenePos));
}

void TopologyScene::continueConnectionDrag(QPointF scenePos) {
  if (drag_line_) {
    QPointF center;
    if (auto* p = qgraphicsitem_cast<PortItem*>(drag_source_))
      center = p->sceneCenter();
    else if (auto* dp = qgraphicsitem_cast<DevicePortItem*>(drag_source_))
      center = dp->sceneCenter();
    QLineF line(center, scenePos);
    drag_line_->setLine(line);
  }
}

void TopologyScene::finishConnectionDrag(QPointF scenePos) {
  if (!drag_source_)
    return;

  // Clean up drag line
  if (drag_line_) {
    removeItem(drag_line_);
    delete drag_line_;
    drag_line_ = nullptr;
  }

  auto* srcPort = qgraphicsitem_cast<PortItem*>(drag_source_);
  auto* srcDevPort = qgraphicsitem_cast<DevicePortItem*>(drag_source_);

  if (srcPort) {
    auto* devPort = devicePortItemAt(scenePos);
    if (devPort) {
      const auto* prod = doc_->product(srcPort->productIndex());
      const auto* dev = doc_->device(devPort->deviceIndex());
      if (prod && dev && devPort->portIndex() < dev->ports.size()) {
        const auto& port = prod->ports[srcPort->portIndex()];
        const auto& dp = dev->ports[devPort->portIndex()];
        if (doc_->canConnect(prod->name, port.name, dev->name, dp.name)) {
          TopologyConnection conn{prod->name, port.name, dev->name, dp.name};
          doc_->undoStack()->push(new AddConnectionCommand(doc_, conn));
        }
      }
    }
  } else if (srcDevPort) {
    auto* uutPort = portItemAt(scenePos);
    if (uutPort) {
      const auto* dev = doc_->device(srcDevPort->deviceIndex());
      const auto* prod = doc_->product(uutPort->productIndex());
      if (dev && prod && uutPort->portIndex() < prod->ports.size()) {
        const auto& dp = dev->ports[srcDevPort->portIndex()];
        const auto& port = prod->ports[uutPort->portIndex()];
        if (doc_->canConnect(prod->name, port.name, dev->name, dp.name)) {
          TopologyConnection conn{prod->name, port.name, dev->name, dp.name};
          doc_->undoStack()->push(new AddConnectionCommand(doc_, conn));
        }
      }
    }
  }

  drag_source_ = nullptr;
}

void TopologyScene::onItemMoved() {
  for (auto* conn : connection_items_) {
    if (conn)
      conn->updatePath();
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

PortItem* TopologyScene::portItemAt(QPointF scenePos) const {
  auto items = this->items(scenePos, Qt::IntersectsItemBoundingRect,
                           Qt::DescendingOrder);
  for (auto* item : items) {
    if (auto* p = qgraphicsitem_cast<PortItem*>(item)) {
      return p;
    }
  }
  return nullptr;
}

UutItem* TopologyScene::findUutItem(int productIndex) const {
  for (auto* uut : uut_items_) {
    if (uut && uut->productIndex() == productIndex)
      return uut;
  }
  return nullptr;
}

DeviceItem* TopologyScene::findDeviceItem(int deviceIndex) const {
  for (auto* dev : device_items_) {
    if (dev && dev->deviceIndex() == deviceIndex)
      return dev;
  }
  return nullptr;
}

ConnectionItem* TopologyScene::connectionItemAt(QPointF scenePos) const {
  auto items = this->items(scenePos, Qt::IntersectsItemBoundingRect,
                           Qt::DescendingOrder);
  for (auto* item : items) {
    if (auto* conn = qgraphicsitem_cast<ConnectionItem*>(item)) {
      return conn;
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
  drag_source_ = nullptr;
  drag_line_ = nullptr;
  moving_item_ = nullptr;
  clear();
  uut_items_.clear();
  device_items_.clear();
  connection_items_.clear();
}

void TopologyScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  QGraphicsScene::mousePressEvent(event);

  if (event->button() == Qt::LeftButton) {
    auto selected = selectedItems();

    // Track moving item BEFORE signal emission — handlers may trigger scene
    // rebuild (e.g. PropertyPanelWidget saving device edits), which would
    // delete items and invalidate pointers in `selected`.
    for (auto* item : selected) {
      if (item->flags() & QGraphicsItem::ItemIsMovable) {
        moving_item_ = item;
        move_start_pos_ = item->pos();
        break;
      }
    }

    if (!selected.isEmpty()) {
      emit itemSelected(selected.first());
    } else {
      emit itemSelected(nullptr);
    }

    // If a signal handler rebuilt the scene, the tracked pointer is now
    // dangling — reset so mouseReleaseEvent won't use it.
    if (moving_item_ && !moving_item_->scene()) {
      moving_item_ = nullptr;
    }
  }
}

void TopologyScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  QGraphicsScene::mouseReleaseEvent(event);

  if (moving_item_ && moving_item_->pos() != move_start_pos_) {
    if (auto* uut = qgraphicsitem_cast<UutItem*>(moving_item_)) {
      auto* cmd = new MoveProductCommand(doc_, uut->productIndex(),
                                         move_start_pos_, moving_item_->pos());
      doc_->undoStack()->push(cmd);
    } else if (auto* dev = qgraphicsitem_cast<DeviceItem*>(moving_item_)) {
      auto* cmd = new MoveDeviceCommand(doc_, dev->deviceIndex(),
                                        move_start_pos_, moving_item_->pos());
      doc_->undoStack()->push(cmd);
    }
  }
  moving_item_ = nullptr;
}

}  // namespace etest::topology
