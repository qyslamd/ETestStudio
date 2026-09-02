#include "TopologyScene.h"
#include "Logger.h"
#include "TopologyDocument.h"
#include "TopologyTheme.h"
#include "UndoCommands.h"
#include "topology_items.h"

#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QPainterPath>
#include <QPen>
#include <QtMath>

namespace etest::topology {

static const char kTopologyDeviceMime[] = "application/x-topology-device";

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
  if (!conn) {
    LOG_WARN("TOPOLOGY_UI", "addConnectionItem: connIndex={} 越界", connIndex);
    return nullptr;
  }

  UutPortItem* sourcePort = nullptr;
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

  if (!sourcePort || !targetPort) {
    LOG_WARN("TOPOLOGY_UI",
             "addConnectionItem: 找不到端口 uut(prod)={} port={} dev={} "
             "devport={} 源={} 目标={}",
             conn->productName.toStdString(), conn->portName.toStdString(),
             conn->deviceName.toStdString(), conn->devicePort.toStdString(),
             sourcePort ? "有" : "无", targetPort ? "有" : "无");
    return nullptr;
  }

  auto* item =
      new ConnectionItem(sourcePort, targetPort, conn->devicePort, doc_);
  item->setConnectionIndex(connIndex);
  addItem(item);
  item->setStyle(conn->style);
  item->updatePath();
  connection_items_.append(item);
  LOG_INFO("TOPOLOGY_UI",
           "addConnectionItem #{} 已创建 {}:{} -> {}:{}  路径空={}",
           connIndex, conn->productName.toStdString(),
           conn->portName.toStdString(), conn->deviceName.toStdString(),
           conn->devicePort.toStdString(),
           item->path().isEmpty() ? "是" : "否");
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
    if (auto* p = qgraphicsitem_cast<UutPortItem*>(drag_source_))
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

  auto* srcPort = qgraphicsitem_cast<UutPortItem*>(drag_source_);
  auto* srcDevPort = qgraphicsitem_cast<DevicePortItem*>(drag_source_);

  // 校验通过的连线在此收集，统一在鼠标事件结束后再入栈
  TopologyConnection pending;
  bool pending_valid = false;

  // 注意：TopologyConnection 首字段是 id（UUID），禁止用 4 元聚合初始化
  // （会把 4 个名字错位塞进 id/productName/portName/deviceName，devicePort
  // 落空，导致 addConnectionItem 匹配不到端口、连线画不出来）
  auto buildConn = [](const TopologyProduct* p, const TopologyPort& pt,
                      const TopologyDevice* d, const TopologyDevicePort& dpt) {
    TopologyConnection c;
    c.productName = p->name;
    c.portName = pt.name;
    c.deviceName = d->name;
    c.devicePort = dpt.name;
    return c;
  };

  if (srcPort) {
    auto* devPort = devicePortItemAt(scenePos);
    if (devPort) {
      const auto* prod = doc_->product(srcPort->productIndex());
      const auto* dev = doc_->device(devPort->deviceIndex());
      if (prod && dev && srcPort->portIndex() >= 0 &&
          srcPort->portIndex() < prod->ports.size() &&
          devPort->portIndex() >= 0 &&
          devPort->portIndex() < dev->ports.size()) {
        const auto& port = prod->ports[srcPort->portIndex()];
        const auto& dp = dev->ports[devPort->portIndex()];
        if (doc_->canConnect(prod->name, port.name, dev->name, dp.name)) {
          pending = buildConn(prod, port, dev, dp);
          pending_valid = true;
          LOG_INFO("TOPOLOGY_UI", "连接成功: {}:{} -> {}:{}",
                   prod->name.toStdString(), port.name.toStdString(),
                   dev->name.toStdString(), dp.name.toStdString());
        } else {
          LOG_WARN("TOPOLOGY_UI", "连接不合法: {}:{} -> {}:{}",
                   prod->name.toStdString(), port.name.toStdString(),
                   dev->name.toStdString(), dp.name.toStdString());
        }
      }
    }
  } else if (srcDevPort) {
    auto* uutPort = portItemAt(scenePos);
    if (uutPort) {
      const auto* dev = doc_->device(srcDevPort->deviceIndex());
      const auto* prod = doc_->product(uutPort->productIndex());
      if (dev && prod && srcDevPort->portIndex() >= 0 &&
          srcDevPort->portIndex() < dev->ports.size() &&
          uutPort->portIndex() >= 0 &&
          uutPort->portIndex() < prod->ports.size()) {
        const auto& dp = dev->ports[srcDevPort->portIndex()];
        const auto& port = prod->ports[uutPort->portIndex()];
        if (doc_->canConnect(prod->name, port.name, dev->name, dp.name)) {
          pending = buildConn(prod, port, dev, dp);
          pending_valid = true;
          LOG_INFO("TOPOLOGY_UI", "连接成功: {}:{} -> {}:{}",
                   prod->name.toStdString(), port.name.toStdString(),
                   dev->name.toStdString(), dp.name.toStdString());
        } else {
          LOG_WARN("TOPOLOGY_UI", "连接不合法: {}:{} -> {}:{}",
                   prod->name.toStdString(), port.name.toStdString(),
                   dev->name.toStdString(), dp.name.toStdString());
        }
      }
    }
  }

  drag_source_ = nullptr;

  if (pending_valid) {
    // 入栈会同步触发 QUndoStack::indexChanged → 场景全量重建（clearScene），
    // 若在本函数（端口 mouseReleaseEvent 内）立即执行会删除正在处理事件的
    // 端口 item，导致悬垂崩溃。推迟到事件结束后再 push，重建即为安全上下文。
    QMetaObject::invokeMethod(
        this,
        [this, pending]() {
          LOG_INFO("TOPOLOGY_UI", "延迟入栈连线: {}:{} -> {}:{}",
                   pending.productName.toStdString(),
                   pending.portName.toStdString(),
                   pending.deviceName.toStdString(),
                   pending.devicePort.toStdString());
          doc_->undoStack()->push(new AddConnectionCommand(doc_, pending));
        },
        Qt::QueuedConnection);
  }
}

void TopologyScene::createDragPreview() {
  const qreal kWidth = 120.0;
  constexpr qreal kBaseHeight = 50.0;
  constexpr qreal kPortMargin = 10.0;
  constexpr qreal kPortSpacing = 20.0;
  constexpr qreal kRadius = 10.0;

  int portCount = drag_preview_data_["channelCount"].toInt(1);
  qreal kHeight =
      qMax(kBaseHeight, 2.0 * kPortMargin + portCount * kPortSpacing);

  QPainterPath path;
  path.addRoundedRect(0, 0, kWidth, kHeight, kRadius, kRadius);
  auto* preview = new QGraphicsPathItem(path);
  preview->setPen(QPen(QColor(100, 180, 255, 200), 1.5, Qt::DashLine));
  QColor fill = topologyColors().deviceFill;
  fill.setAlpha(100);
  preview->setBrush(fill);
  preview->setZValue(1000);

  QString name = drag_preview_data_["deviceType"].toString();
  auto* text = new QGraphicsSimpleTextItem(name, preview);
  QFont f = text->font();
  f.setPointSize(10);
  f.setBold(true);
  text->setFont(f);
  text->setBrush(QColor(200, 200, 200, 180));
  text->setPos((kWidth - text->boundingRect().width()) / 2.0,
               (kHeight - text->boundingRect().height()) / 2.0);

  addItem(preview);
  drag_preview_ = preview;
}

void TopologyScene::dragEnterEvent(QGraphicsSceneDragDropEvent* event) {
  if (event->mimeData()->hasFormat(QLatin1String(kTopologyDeviceMime))) {
    // Parse device data and create a ghost preview
    QJsonDocument jdoc = QJsonDocument::fromJson(
        event->mimeData()->data(QLatin1String(kTopologyDeviceMime)));
    if (jdoc.isObject()) {
      drag_preview_data_ = jdoc.object();
      createDragPreview();
      if (drag_preview_) {
        drag_preview_->setPos(event->scenePos());
      }
    }
    event->acceptProposedAction();
    return;
  }
  QGraphicsScene::dragEnterEvent(event);
}

void TopologyScene::dragMoveEvent(QGraphicsSceneDragDropEvent* event) {
  if (event->mimeData()->hasFormat(QLatin1String(kTopologyDeviceMime))) {
    if (drag_preview_) {
      // Preview top-left follows cursor to match drop position
      drag_preview_->setPos(event->scenePos());
    }
    event->acceptProposedAction();
    return;
  }
  QGraphicsScene::dragMoveEvent(event);
}

void TopologyScene::dragLeaveEvent(QGraphicsSceneDragDropEvent* event) {
  if (drag_preview_) {
    removeItem(drag_preview_);
    delete drag_preview_;
    drag_preview_ = nullptr;
    drag_preview_data_ = QJsonObject();
  }
  QGraphicsScene::dragLeaveEvent(event);
}

void TopologyScene::dropEvent(QGraphicsSceneDragDropEvent* event) {
  if (event->mimeData()->hasFormat(QLatin1String(kTopologyDeviceMime))) {
    // Clean up preview
    if (drag_preview_) {
      removeItem(drag_preview_);
      delete drag_preview_;
      drag_preview_ = nullptr;
    }

    QJsonDocument jdoc = QJsonDocument::fromJson(
        event->mimeData()->data(QLatin1String(kTopologyDeviceMime)));
    if (jdoc.isObject()) {
      QJsonObject obj = jdoc.object();
      emit deviceDropped(obj["deviceType"].toString(),
                         obj["channelCount"].toInt(), obj["direction"].toInt(),
                         obj["functionType"].toInt(),
                         obj["pluginId"].toString(), event->scenePos());
    }
    drag_preview_data_ = QJsonObject();
    event->acceptProposedAction();
    return;
  }
  QGraphicsScene::dropEvent(event);
}

void TopologyScene::onItemMoved() {
  for (auto* conn : connection_items_) {
    if (conn)
      conn->updatePath();
  }
}

DeviceItem* TopologyScene::deviceItemAt(QPointF scenePos) const {
  auto items =
      this->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder);
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

UutPortItem* TopologyScene::portItemAt(QPointF scenePos) const {
  auto items = this->items(scenePos, Qt::IntersectsItemBoundingRect,
                           Qt::DescendingOrder);
  for (auto* item : items) {
    if (auto* p = qgraphicsitem_cast<UutPortItem*>(item)) {
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
  auto items =
      this->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder);
  for (auto* item : items) {
    if (auto* conn = qgraphicsitem_cast<ConnectionItem*>(item)) {
      return conn;
    }
  }
  return nullptr;
}

ConnectionItem* TopologyScene::findConnectionItem(int connIndex) const {
  for (auto* conn : connection_items_) {
    if (conn && conn->connectionIndex() == connIndex)
      return conn;
  }
  return nullptr;
}

UutItem* TopologyScene::uutItemAt(QPointF scenePos) const {
  auto items =
      this->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder);
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
  drag_preview_ = nullptr;
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
