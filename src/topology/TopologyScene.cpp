#include "TopologyScene.h"
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
    : QGraphicsScene(parent), doc_(doc) {
}

void TopologyScene::loadFromDocument() {
  bool wasMonitorView = monitor_view_active_;
  clearScene();
  monitor_view_active_ = wasMonitorView;

  for (int i = 0; i < doc_->productCount(); ++i) {
    const auto* prod = doc_->product(i);
    addProductItem(i, prod->position);
  }
  for (int i = 0; i < doc_->deviceCount(); ++i) {
    const auto* dev = doc_->device(i);
    addDeviceItem(i, dev->position);
  }
  for (int i = 0; i < doc_->monitorCount(); ++i) {
    const auto* mon = doc_->monitor(i);
    addMonitorItem(i, mon->position);
  }
  for (int i = 0; i < doc_->connectionCount(); ++i) {
    addConnectionItem(i);
  }
  updateTapVisuals();
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
  for (auto* mon : monitor_items_) {
    if (auto* m = doc_->monitor(mon->monitorIndex())) {
      m->position = mon->pos();
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

MonitorItem* TopologyScene::addMonitorItem(int monitorIndex, const QPointF& pos) {
  auto* item = new MonitorItem(monitorIndex, doc_);
  item->setPos(pos);
  addItem(item);
  monitor_items_.append(item);
  return item;
}

// ── Tap mode ────────────────────────────────────────────────

void TopologyScene::startTapMode(int monitorIndex) {
  if (!monitor_view_active_)
    return;
  tap_mode_monitor_ = monitorIndex;
  // Use cross cursor to indicate tap selection mode
  for (auto* view : views())
    view->setCursor(Qt::CrossCursor);
}

void TopologyScene::finishTap(QPointF scenePos) {
  if (tap_mode_monitor_ < 0)
    return;

  // Find a connection at the click position
  auto* conn = connectionItemAt(scenePos);
  if (conn) {
    const auto* c = doc_->connection(conn->connectionIndex());
    if (c) {
      TopologyMonitorTap tap;
      tap.productName = c->productName;
      tap.portName = c->portName;
      tap.deviceName = c->deviceName;
      tap.devicePort = c->devicePort;

      // Check it's not already tapped by this monitor
      const auto* mon = doc_->monitor(tap_mode_monitor_);
      bool alreadyTapped = false;
      if (mon) {
        for (const auto& t : mon->taps) {
          if (t.productName == tap.productName &&
              t.portName == tap.portName &&
              t.deviceName == tap.deviceName &&
              t.devicePort == tap.devicePort) {
            alreadyTapped = true;
            break;
          }
        }
      }

      if (!alreadyTapped) {
        auto* cmd = new TapConnectionCommand(doc_, tap_mode_monitor_, tap);
        doc_->undoStack()->push(cmd);
      }
    }
  }

  cancelTapMode();
}

void TopologyScene::cancelTapMode() {
  tap_mode_monitor_ = -1;
  for (auto* view : views())
    view->unsetCursor();
}

// ── Tap visual indicators ────────────────────────────────────

void TopologyScene::setMonitorViewActive(bool active) {
  monitor_view_active_ = active;
  if (!active) {
    // Cancel any in-progress tap mode
    if (tap_mode_monitor_ >= 0)
      cancelTapMode();
  }
  updateTapVisuals();
}

void TopologyScene::updateTapVisuals() {
  // Remove old tap lines
  for (auto* line : tap_lines_) {
    removeItem(line);
    delete line;
  }
  tap_lines_.clear();

  if (!monitor_view_active_)
    return;

  // Build a lookup: connection endpoint → monitorItem + tap index
  for (auto* monItem : monitor_items_) {
    if (!monItem)
      continue;
    const auto* mon = doc_->monitor(monItem->monitorIndex());
    if (!mon)
      continue;

    QPointF monCenter = monItem->sceneBoundingRect().center();

    for (const auto& tap : mon->taps) {
      // Find the ConnectionItem matching this tap
      for (auto* connItem : connection_items_) {
        if (!connItem)
          continue;
        const auto* c = doc_->connection(connItem->connectionIndex());
        if (!c)
          continue;
        if (c->productName == tap.productName &&
            c->portName == tap.portName &&
            c->deviceName == tap.deviceName &&
            c->devicePort == tap.devicePort) {
          // Find closest point on the connection path to monitor center
          QPainterPath path = connItem->path();
          qreal bestParam = 0;
          qreal bestDist = 1e18;
          // Sample the path at 20 points
          for (int i = 0; i <= 20; ++i) {
            qreal t = i / 20.0;
            QPointF pt = path.pointAtPercent(t);
            qreal d = QLineF(pt, monCenter).length();
            if (d < bestDist) {
              bestDist = d;
              bestParam = t;
            }
          }
          QPointF tapPt = path.pointAtPercent(bestParam);

          // Draw dotted line from tap point to monitor
          auto* line = new QGraphicsLineItem(QLineF(tapPt, monCenter));
          line->setPen(QPen(QColor(180, 130, 255, 180), 1.5, Qt::DashLine));
          line->setZValue(4);
          addItem(line);
          tap_lines_.append(line);

          // Draw a small diamond at the tap point
          // (handled by painting a small marker — for now just the line suffices)
          break;
        }
      }
    }
  }
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

void TopologyScene::createDragPreview() {
  const qreal kWidth = 120.0;
  constexpr qreal kBaseHeight = 50.0;
  constexpr qreal kPortMargin = 10.0;
  constexpr qreal kPortSpacing = 20.0;
  constexpr qreal kRadius = 10.0;

  int portCount = drag_preview_data_["channelCount"].toInt(1);
  qreal kHeight = qMax(kBaseHeight, 2.0 * kPortMargin + portCount * kPortSpacing);

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
      if (obj["isMonitor"].toBool()) {
        emit monitorDropped(obj["deviceType"].toString(),
                            event->scenePos());
      } else {
        emit deviceDropped(obj["deviceType"].toString(),
                           obj["channelCount"].toInt(),
                           obj["direction"].toInt(),
                           obj["functionType"].toInt(),
                           event->scenePos());
      }
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
  updateTapVisuals();
}

DeviceItem* TopologyScene::deviceItemAt(QPointF scenePos) const {
  auto items = this->items(scenePos, Qt::IntersectsItemShape,
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
  auto items = this->items(scenePos, Qt::IntersectsItemShape,
                           Qt::DescendingOrder);
  for (auto* item : items) {
    if (auto* dp = qgraphicsitem_cast<DevicePortItem*>(item)) {
      return dp;
    }
  }
  return nullptr;
}

PortItem* TopologyScene::portItemAt(QPointF scenePos) const {
  auto items = this->items(scenePos, Qt::IntersectsItemShape,
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
  auto items = this->items(scenePos, Qt::IntersectsItemShape,
                           Qt::DescendingOrder);
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

MonitorItem* TopologyScene::findMonitorItem(int monitorIndex) const {
  for (auto* mon : monitor_items_) {
    if (mon && mon->monitorIndex() == monitorIndex)
      return mon;
  }
  return nullptr;
}

UutItem* TopologyScene::uutItemAt(QPointF scenePos) const {
  auto items = this->items(scenePos, Qt::IntersectsItemShape,
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
  drag_preview_ = nullptr;
  moving_item_ = nullptr;
  tap_mode_monitor_ = -1;
  monitor_view_active_ = false;
  tap_lines_.clear();  // Items will be deleted by clear()
  clear();
  uut_items_.clear();
  device_items_.clear();
  connection_items_.clear();
  monitor_items_.clear();
}

void TopologyScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  // Tap mode — clicking on a connection creates a tap
  if (event->button() == Qt::LeftButton && tap_mode_monitor_ >= 0) {
    finishTap(event->scenePos());
    event->accept();
    return;
  }

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
    } else if (auto* mon = qgraphicsitem_cast<MonitorItem*>(moving_item_)) {
      auto* cmd = new MoveMonitorCommand(doc_, mon->monitorIndex(),
                                         move_start_pos_, moving_item_->pos());
      doc_->undoStack()->push(cmd);
    }
  }
  moving_item_ = nullptr;
}

}  // namespace etest::topology
