#define _USE_MATH_DEFINES
#include <cmath>

#include "TopologyDocument.h"
#include "TopologyScene.h"
#include "TopologyTheme.h"
#include "topology_items.h"

#include <QAction>
#include <QActionGroup>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPointer>
#include <QStyleOptionGraphicsItem>
#include <QTimer>

#include "UndoCommands.h"

namespace etest::topology {

constexpr qreal kLineLength = 28.0;
constexpr qreal kEndRadius = 3.0;
constexpr qreal kPortRadius = 6.0;

// ═══════════════════════════════════════════════════════════════
//  AbstractPortItem ── shared port node logic
// ═══════════════════════════════════════════════════════════════

AbstractPortItem::AbstractPortItem(int portIndex,
                                   TopologyDocument* doc,
                                   QGraphicsItem* parent)
    : QGraphicsItem(parent),
      port_index_(portIndex),
      doc_(doc) {
  setAcceptHoverEvents(true);
  setCursor(Qt::CrossCursor);
  setFlag(ItemIsSelectable);
}

void AbstractPortItem::hoverEnterEvent(QGraphicsSceneHoverEvent*) {
  hovered_ = true;
  update();
}

void AbstractPortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*) {
  hovered_ = false;
  update();
}

void AbstractPortItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  QGraphicsItem::mousePressEvent(event);
  press_pos_ = event->scenePos();
  event->accept();
}

void AbstractPortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if ((event->scenePos() - press_pos_).manhattanLength() > 5) {
    if (auto* s = qobject_cast<TopologyScene*>(scene())) {
      s->startConnectionDrag(this, press_pos_);
      s->continueConnectionDrag(event->scenePos());
    }
  }
  event->accept();
}

void AbstractPortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (auto* s = qobject_cast<TopologyScene*>(scene())) {
    s->finishConnectionDrag(event->scenePos());
  }
  event->accept();
}

// ═══════════════════════════════════════════════════════════════
//  UutPortItem ── pin node on UUT edge
// ═══════════════════════════════════════════════════════════════

UutPortItem::UutPortItem(int productIndex,
                          int portIndex,
                          TopologyDocument* doc,
                          UutItem* parent)
    : AbstractPortItem(portIndex, doc, parent),
      product_index_(productIndex) {
}

void UutPortItem::setPortStyle(PortStyle s) {
  port_style_ = s;
  if (auto* prod = doc_->product(product_index_)) {
    if (port_index_ < prod->ports.size())
      prod->ports[port_index_].portStyle = static_cast<int>(s);
  }
  update();
}

QRectF UutPortItem::boundingRect() const {
  return QRectF(-kLineLength - kEndRadius - 80, -16,
                kLineLength * 2 + kEndRadius * 2 + 80 * 2, 30);
}

QPainterPath UutPortItem::shape() const {
  QPainterPath p;
  p.addEllipse(-kRadius, -kRadius, kRadius * 2, kRadius * 2);
  p.addEllipse(-kLineLength - kRadius, -kRadius, kRadius * 2, kRadius * 2);
  QPainterPath line;
  line.moveTo(0, 0);
  line.lineTo(-kLineLength, 0);
  QPainterPathStroker stroker;
  stroker.setWidth(18.0);
  p.addPath(stroker.createStroke(line));
  return p;
}

static QColor directionColor(TopologyPort::Direction d) {
  const auto& tc = topologyColors();
  switch (d) {
    case TopologyPort::Direction::Input:
      return tc.directionInput;
    case TopologyPort::Direction::Output:
      return tc.directionOutput;
    case TopologyPort::Direction::Bidirectional:
      return tc.directionBidirectional;
  }
  return QColor(128, 128, 128);
}

void UutPortItem::paint(QPainter* painter,
                        const QStyleOptionGraphicsItem* option,
                        QWidget*) {
  painter->setRenderHint(QPainter::Antialiasing);

  const auto* prod = doc_->product(product_index_);
  if (!prod || port_index_ >= prod->ports.size())
    return;
  const auto& port = prod->ports[port_index_];

  QColor color = directionColor(port.direction);
  qreal penWidth = 1.5;
  qreal dotRadius = kRadius;

  if (hovered_) {
    color = color.darker(115);
    penWidth = 2.0;
    dotRadius = 7.0;
  }
  if (option->state & QStyle::State_Selected) {
    color = color.lighter(150);
    penWidth = 2.5;
    dotRadius = 8.0;
  }

  qreal lineEndX = -kLineLength;

  // Line from block edge to endpoint
  painter->setPen(QPen(color, penWidth));
  painter->drawLine(QPointF(0, 0), QPointF(lineEndX, 0));

  // Port symbol at endpoint: ---O  (circle or triangle)
  painter->setBrush(color);
  if (port_style_ == PortStyle::Circle) {
    painter->setPen(QPen(color.darker(140), penWidth));
    painter->drawEllipse(QPointF(lineEndX, 0), dotRadius, dotRadius);
  } else {
    qreal hh = dotRadius * 0.85;
    qreal hw = dotRadius * 1.1;
    QPainterPath portPath;
    if (port.direction == TopologyPort::Direction::Bidirectional) {
      portPath.moveTo(lineEndX + hw, 0);
      portPath.lineTo(lineEndX, -hh);
      portPath.lineTo(lineEndX - hw, 0);
      portPath.lineTo(lineEndX, hh);
      portPath.closeSubpath();
    } else {
      // Input: tip points toward block (+x); Output: tip toward line (-x)
      qreal tipX = (port.direction == TopologyPort::Direction::Input)
                       ? lineEndX + hw : lineEndX - hw;
      qreal baseX = 2 * lineEndX - tipX;
      portPath.moveTo(tipX, 0);
      portPath.lineTo(baseX, -hh);
      portPath.lineTo(baseX, hh);
      portPath.closeSubpath();
    }
    painter->setBrush(color);
    painter->setPen(QPen(color.darker(140), penWidth));
    painter->drawPath(portPath);
  }

  painter->setPen(topologyColors().textPrimary);
  QFont f = painter->font();
  f.setPointSize(7);
  painter->setFont(f);

  QString label = port.name;
  qreal tw = painter->fontMetrics().horizontalAdvance(label);
  qreal midX = lineEndX / 2.0;
  painter->drawText(QPointF(midX - tw / 2.0, -8), label);
}

QPointF UutPortItem::sceneCenter() const {
  return mapToScene(QPointF(-kLineLength, 0));
}

// ═══════════════════════════════════════════════════════════════
//  UutItem
// ═══════════════════════════════════════════════════════════════

UutItem::UutItem(int productIndex, TopologyDocument* doc, QGraphicsItem* parent)
    : TopologyBlockItem(doc, kWidth, kCornerRadius, parent),
      product_index_(productIndex) {
  if (auto* prod = doc->product(productIndex)) {
    if (prod->size.isValid() && prod->size.width() > 0) {
      block_width_ = prod->size.width();
      block_height_ = prod->size.height();
    }
  }
  layoutPorts();
}

void UutItem::paintContent(QPainter* painter,
                           const QStyleOptionGraphicsItem*,
                           const QRectF& rect) {
  const auto* prod = doc_->product(product_index_);
  if (!prod)
    return;
  painter->setPen(topologyColors().textPrimary);
  QFont f = painter->font();
  f.setPointSize(10);
  f.setBold(true);
  painter->setFont(f);
  painter->drawText(rect, Qt::AlignCenter, prod->name);
}

qreal UutItem::calcContentHeight() const {
  const auto* prod = doc_->product(product_index_);
  int portCount = prod ? prod->ports.size() : 0;
  constexpr qreal kPortSpacing = 20.0;
  return qMax(kBaseHeight, 2 * kPortMargin + portCount * kPortSpacing);
}

bool UutItem::hasChildHovered() const {
  for (auto* port : ports_) {
    if (port->isVisible() && port->isHovered())
      return true;
  }
  return false;
}

QColor UutItem::blockFillColor() const {
  return topologyColors().uutFill;
}

QColor UutItem::blockBorderColor() const {
  return topologyColors().uutBorder;
}

void UutItem::layoutPorts() {
  ports_.clear();
  const auto* prod = doc_->product(product_index_);
  if (!prod)
    return;

  qreal h = effectiveHeight();
  int n = prod->ports.size();
  for (int i = 0; i < n; ++i) {
    auto* portItem = new UutPortItem(product_index_, i, doc_, this);
    portItem->setPortStyle(static_cast<PortStyle>(prod->ports[i].portStyle));
    ports_.append(portItem);
    addChildPort(portItem);

    qreal y;
    if (n <= 1) {
      y = h / 2.0;
    } else {
      y = kPortMargin + i * (h - 2 * kPortMargin) / (n - 1);
    }
    portItem->setPos(-kPortRadius, y);
  }
}

UutPortItem* UutItem::portItem(int portIndex) const {
  if (portIndex < 0 || portIndex >= ports_.size())
    return nullptr;
  return ports_[portIndex];
}

QPointF UutItem::portScenePos(int portIndex) const {
  auto* pi = portItem(portIndex);
  return pi ? pi->sceneCenter() : QPointF();
}

void UutItem::clearPorts() {
  for (auto* port : ports_) {
    if (port->scene())
      port->scene()->removeItem(port);
    delete port;
  }
  ports_.clear();
  clearChildPorts();
}

void UutItem::onResizeFinished(const QSizeF&, const QPointF& oldPos) {
  auto* prod = doc_->product(product_index_);
  if (!prod)
    return;

  QSizeF oldSize = prod->size;
  QSizeF newSize(block_width_, block_height_);
  if (oldSize == newSize)
    return;

  prod->size = newSize;

  int idx = product_index_;
  QPointF newPos = pos();
  QPointer<TopologyDocument> doc = doc_;
  QTimer::singleShot(0, [doc, idx, oldSize, newSize, oldPos, newPos]() {
    if (!doc)
      return;
    doc->undoStack()->push(new ResizeItemCommand(
        doc, idx, ResizeItemCommand::Product, oldSize, newSize, oldPos,
        newPos));
  });
}

// ═══════════════════════════════════════════════════════════════
//  DeviceItem
// ═══════════════════════════════════════════════════════════════

DeviceItem::DeviceItem(int deviceIndex,
                       TopologyDocument* doc,
                       QGraphicsItem* parent)
    : TopologyBlockItem(doc, kWidth, kCornerRadius, parent),
      device_index_(deviceIndex) {
  if (auto* dev = doc->device(deviceIndex)) {
    if (dev->size.isValid() && dev->size.width() > 0) {
      block_width_ = dev->size.width();
      block_height_ = dev->size.height();
    }
  }
  layoutDevicePorts();
}

void DeviceItem::paintContent(QPainter* painter,
                              const QStyleOptionGraphicsItem*,
                              const QRectF& rect) {
  const auto* dev = doc_->device(device_index_);
  if (!dev)
    return;

  painter->setPen(topologyColors().textPrimary);
  QFont f = painter->font();
  f.setPointSize(9);
  f.setBold(true);
  painter->setFont(f);
  painter->drawText(QRectF(rect.x(), rect.y() + 6, rect.width(), 20),
                    Qt::AlignCenter, dev->name);

  f.setPointSize(7);
  f.setBold(false);
  painter->setFont(f);
  painter->setPen(topologyColors().textSecondary);
  painter->drawText(QRectF(rect.x(), rect.y() + 26, rect.width(), 18),
                    Qt::AlignCenter,
                    QStringLiteral("[%1]").arg(dev->deviceType));
}

qreal DeviceItem::calcContentHeight() const {
  const auto* dev = doc_->device(device_index_);
  int portCount = dev ? dev->ports.size() : 0;
  constexpr qreal kPortSpacing = 20.0;
  return qMax(kBaseHeight, 2 * kPortMargin + portCount * kPortSpacing);
}

bool DeviceItem::hasChildHovered() const {
  for (auto* port : device_port_items_) {
    if (port->isVisible() && port->isHovered())
      return true;
  }
  return false;
}

QColor DeviceItem::blockFillColor() const {
  return topologyColors().deviceFill;
}

QColor DeviceItem::blockBorderColor() const {
  return topologyColors().deviceBorder;
}

QString DeviceItem::deviceType() const {
  const auto* dev = doc_->device(device_index_);
  return dev ? dev->deviceType : QString();
}

QPointF DeviceItem::connectionPoint() const {
  return mapToScene(QPointF(block_width_, effectiveHeight() / 2.0));
}

void DeviceItem::layoutDevicePorts() {
  for (auto* p : device_port_items_) {
    if (scene())
      scene()->removeItem(p);
    delete p;
  }
  device_port_items_.clear();
  clearChildPorts();

  const auto* dev = doc_->device(device_index_);
  if (!dev)
    return;

  qreal h = effectiveHeight();
  int n = dev->ports.size();
  for (int i = 0; i < n; ++i) {
    auto* portItem = new DevicePortItem(device_index_, i, doc_, this);
    portItem->setPortStyle(static_cast<PortStyle>(dev->ports[i].portStyle));
    device_port_items_.append(portItem);
    addChildPort(portItem);

    qreal y;
    if (n <= 1) {
      y = h / 2.0;
    } else {
      y = kPortMargin + i * (h - 2 * kPortMargin) / (n - 1);
    }
    portItem->setPos(block_width_ + kPortRadius, y);
  }
}

DevicePortItem* DeviceItem::devicePortItem(int portIndex) const {
  if (portIndex < 0 || portIndex >= device_port_items_.size())
    return nullptr;
  return device_port_items_[portIndex];
}

void DeviceItem::onResizeFinished(const QSizeF&, const QPointF& oldPos) {
  auto* dev = doc_->device(device_index_);
  if (!dev)
    return;

  QSizeF oldSize = dev->size;
  QSizeF newSize(block_width_, block_height_);
  if (oldSize == newSize)
    return;

  dev->size = newSize;

  int idx = device_index_;
  QPointF newPos = pos();
  QPointer<TopologyDocument> doc = doc_;
  QTimer::singleShot(0, [doc, idx, oldSize, newSize, oldPos, newPos]() {
    if (!doc)
      return;
    doc->undoStack()->push(new ResizeItemCommand(
        doc, idx, ResizeItemCommand::Device, oldSize, newSize, oldPos,
        newPos));
  });
}

// ═══════════════════════════════════════════════════════════════
//  DevicePortItem
// ═══════════════════════════════════════════════════════════════

DevicePortItem::DevicePortItem(int deviceIndex,
                               int portIndex,
                               TopologyDocument* doc,
                               DeviceItem* parent)
    : AbstractPortItem(portIndex, doc, parent),
      device_index_(deviceIndex) {
}

void DevicePortItem::setPortStyle(PortStyle s) {
  port_style_ = s;
  if (auto* dev = doc_->device(device_index_)) {
    if (port_index_ < dev->ports.size())
      dev->ports[port_index_].portStyle = static_cast<int>(s);
  }
  update();
}

QRectF DevicePortItem::boundingRect() const {
  return QRectF(-kRadius - 4, -16, kLineLength + kEndRadius + kRadius + 80 + 4,
                30);
}

QPainterPath DevicePortItem::shape() const {
  QPainterPath p;
  p.addEllipse(-kRadius, -kRadius, kRadius * 2, kRadius * 2);
  p.addEllipse(kLineLength - kRadius, -kRadius, kRadius * 2, kRadius * 2);
  QPainterPath line;
  line.moveTo(0, 0);
  line.lineTo(kLineLength, 0);
  QPainterPathStroker stroker;
  stroker.setWidth(18.0);
  p.addPath(stroker.createStroke(line));
  return p;
}

void DevicePortItem::paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget*) {
  painter->setRenderHint(QPainter::Antialiasing);

  const auto* dev = doc_->device(device_index_);
  if (!dev || port_index_ >= dev->ports.size())
    return;
  const auto& port = dev->ports[port_index_];

  QColor color = directionColor(port.direction);
  qreal penWidth = 1.5;
  qreal dotRadius = kRadius;

  if (hovered_) {
    color = color.darker(115);
    penWidth = 2.0;
    dotRadius = 7.0;
  }
  if (option->state & QStyle::State_Selected) {
    color = color.lighter(150);
    penWidth = 2.5;
    dotRadius = 8.0;
  }

  qreal lineEndX = kLineLength;

  painter->setPen(QPen(color, penWidth));
  painter->drawLine(QPointF(0, 0), QPointF(lineEndX, 0));

  // Port symbol at endpoint: ---O (circle or triangle)
  if (port_style_ == PortStyle::Circle) {
    painter->setBrush(color);
    painter->setPen(QPen(color.darker(140), penWidth));
    painter->drawEllipse(QPointF(lineEndX, 0), dotRadius, dotRadius);
  } else {
    qreal hh = dotRadius * 0.85;
    qreal hw = dotRadius * 1.1;
    QPainterPath portPath;
    if (port.direction == TopologyPort::Direction::Bidirectional) {
      portPath.moveTo(lineEndX + hw, 0);
      portPath.lineTo(lineEndX, -hh);
      portPath.lineTo(lineEndX - hw, 0);
      portPath.lineTo(lineEndX, hh);
      portPath.closeSubpath();
    } else {
      // Input: tip points into body (-x); Output: tip toward line (+x)
      qreal tipX = (port.direction == TopologyPort::Direction::Input)
                       ? lineEndX - hw : lineEndX + hw;
      qreal baseX = 2 * lineEndX - tipX;
      portPath.moveTo(tipX, 0);
      portPath.lineTo(baseX, -hh);
      portPath.lineTo(baseX, hh);
      portPath.closeSubpath();
    }
    painter->setBrush(color);
    painter->setPen(QPen(color.darker(140), penWidth));
    painter->drawPath(portPath);
  }

  painter->setPen(topologyColors().textPrimary);
  QFont f = painter->font();
  f.setPointSize(7);
  painter->setFont(f);
  QString label = QStringLiteral("%1 [%2]").arg(
      port.name, functionTypeToString(port.functionType));
  qreal tw = painter->fontMetrics().horizontalAdvance(label);
  qreal midX = lineEndX / 2.0;
  painter->drawText(QPointF(midX - tw / 2.0, -8), label);
}

DeviceItem* DevicePortItem::parentDeviceItem() const {
  return qgraphicsitem_cast<DeviceItem*>(parentItem());
}

QPointF DevicePortItem::sceneCenter() const {
  return mapToScene(QPointF(kLineLength, 0));
}

// ═══════════════════════════════════════════════════════════════
//  ConnectionItem
// ═══════════════════════════════════════════════════════════════

ConnectionItem::ConnectionItem(UutPortItem* source,
                               DevicePortItem* target,
                               const QString& devicePort,
                               TopologyDocument* doc,
                               QGraphicsItem* parent)
    : QGraphicsPathItem(parent),
      source_(source),
      target_port_(target),
      device_port_(devicePort),
      doc_(doc) {
  setFlag(ItemIsSelectable);
  setAcceptHoverEvents(true);
  setZValue(-1);
}

ConnectionItem::~ConnectionItem() {}

QRectF ConnectionItem::boundingRect() const {
  constexpr qreal kArrowMargin = 12.0;
  return QGraphicsPathItem::boundingRect().adjusted(
      -kArrowMargin, -kArrowMargin, kArrowMargin, kArrowMargin);
}

QPainterPath ConnectionItem::shape() const {
  QPainterPath p = path();
  if (p.isEmpty()) {
    QPainterPath empty;
    empty.addEllipse(QPointF(), 1, 1);
    return empty;
  }
  QPainterPathStroker stroker;
  stroker.setWidth(12.0);
  QPainterPath s = stroker.createStroke(p);
  s.addPath(arrow_path_);
  return s;
}

void ConnectionItem::updatePath() {
  if (!source_ || !target_port_)
    return;

  QPointF start = source_->sceneCenter();
  QPointF end = target_port_->sceneCenter();

  // Gather obstacle rects (UUT / Device blocks), excluding source/target
  // parents
  QVector<QRectF> obstacles;
  if (auto* sc = scene()) {
    QGraphicsItem* srcParent = source_->parentItem();
    QGraphicsItem* tgtParent = target_port_->parentItem();
    const auto allItems = sc->items();
    for (auto* item : allItems) {
      if (item == srcParent || item == tgtParent)
        continue;
      if (qgraphicsitem_cast<UutItem*>(item) ||
          qgraphicsitem_cast<DeviceItem*>(item)) {
        // Expand slightly so the path doesn't hug the block
        obstacles.append(item->sceneBoundingRect().adjusted(-6, -6, 6, 6));
      }
    }
  }

  TopologyPathRouter::Context ctx;
  ctx.sourcePos = start;
  ctx.targetPos = end;
  ctx.style = style_;
  ctx.obstacles = obstacles;

  TopologyPathRouter router;
  QPainterPath p = router.route(ctx);
  setPath(p);

  // Compute direction arrow at endpoint near relevant side
  arrow_path_ = QPainterPath();
  const auto* dev = doc_->device(target_port_->deviceIndex());
  if (!dev || target_port_->portIndex() >= dev->ports.size())
    return;
  auto dir = dev->ports[target_port_->portIndex()].direction;

  // Arrow at t≈0 (UUT side) or t≈1 (Device side) depending on flow
  qreal t = (dir == TopologyPort::Direction::Output) ? 0.05 : 0.95;
  QPointF pos = p.pointAtPercent(t);
  QPointF tang = p.pointAtPercent(t + 0.01) - p.pointAtPercent(t - 0.01);
  qreal angle = atan2(tang.y(), tang.x());

  qreal as = 12.0;
  qreal gap = 10.0;
  if (dir == TopologyPort::Direction::Bidirectional) {
    // Arrow at UUT side pointing toward UUT (←)
    qreal a = angle + M_PI;
    QPointF revCenter = p.pointAtPercent(0.12) + QPointF(cos(a) * gap, sin(a) * gap);
    arrow_path_.moveTo(revCenter + QPointF(cos(a) * as, sin(a) * as));
    arrow_path_.lineTo(revCenter +
                       QPointF(cos(a + 2.5) * as, sin(a + 2.5) * as));
    arrow_path_.lineTo(revCenter +
                       QPointF(cos(a - 2.5) * as, sin(a - 2.5) * as));
    arrow_path_.closeSubpath();
    // Arrow at Device side pointing toward Device (→)
    a = angle;
    QPointF fwdCenter = p.pointAtPercent(0.88) + QPointF(cos(a) * gap, sin(a) * gap);
    arrow_path_.moveTo(fwdCenter + QPointF(cos(a) * as, sin(a) * as));
    arrow_path_.lineTo(fwdCenter +
                       QPointF(cos(a + 2.5) * as, sin(a + 2.5) * as));
    arrow_path_.lineTo(fwdCenter +
                       QPointF(cos(a - 2.5) * as, sin(a - 2.5) * as));
    arrow_path_.closeSubpath();
  } else {
    // Output → arrow near UUT side pointing toward UUT (reverse)
    // Input  → arrow near Device side pointing toward Device (forward)
    bool reverse = (dir == TopologyPort::Direction::Output);
    qreal a = reverse ? angle + M_PI : angle;
    arrow_path_.moveTo(pos + QPointF(cos(a) * as, sin(a) * as));
    arrow_path_.lineTo(pos + QPointF(cos(a + 2.5) * as, sin(a + 2.5) * as));
    arrow_path_.lineTo(pos + QPointF(cos(a - 2.5) * as, sin(a - 2.5) * as));
    arrow_path_.closeSubpath();
  }
}

void ConnectionItem::paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) {
  Q_UNUSED(widget);
  painter->setRenderHint(QPainter::Antialiasing);

  const auto& tc = topologyColors();
  QColor lineColor = tc.connectionLine;
  qreal penWidth = 1.5;

  if (option->state & QStyle::State_MouseOver) {
    lineColor = tc.connectionHover;
    penWidth = 2.0;
  }
  if (option->state & QStyle::State_Selected) {
    lineColor = tc.connectionSelected;
    penWidth = 2.5;
  }

  painter->setPen(QPen(lineColor, penWidth));
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(path());

  if (arrow_path_.isEmpty())
    return;
  const auto* dev = doc_->device(target_port_->deviceIndex());
  if (!dev || target_port_->portIndex() >= dev->ports.size())
    return;
  auto dir = dev->ports[target_port_->portIndex()].direction;

  QColor arrowColor = directionColor(dir);
  if (option->state & (QStyle::State_MouseOver | QStyle::State_Selected)) {
    arrowColor = lineColor;
  }

  painter->setPen(Qt::NoPen);
  painter->setBrush(arrowColor);
  painter->drawPath(arrow_path_);
}

DeviceItem* ConnectionItem::targetDevice() const {
  return target_port_ ? target_port_->parentDeviceItem() : nullptr;
}

void ConnectionItem::setStyle(PathStyle s) {
  if (style_ == s)
    return;
  style_ = s;
  if (doc_ && conn_index_ >= 0) {
    if (auto* c = doc_->connection(conn_index_)) {
      c->style = s;
    }
  }
  updatePath();
  update();
}

// ═══════════════════════════════════════════════════════════════
//  LegendItem
// ═══════════════════════════════════════════════════════════════

LegendItem::LegendItem(QGraphicsItem* parent) : QGraphicsItem(parent) {
  setFlag(ItemIsSelectable, false);
  setZValue(100);
}

QRectF LegendItem::boundingRect() const {
  return QRectF(0, 0, 100, 70);
}

void LegendItem::paint(QPainter* painter,
                       const QStyleOptionGraphicsItem*,
                       QWidget*) {
  painter->setRenderHint(QPainter::Antialiasing);
  const auto& tc = topologyColors();

  // Background
  painter->setBrush(tc.legendBackground);
  painter->setPen(QPen(tc.legendBorder, 0.5));
  painter->drawRoundedRect(boundingRect().adjusted(1, 1, -1, -1), 4, 4);

  QFont f = painter->font();
  f.setPointSize(8);
  f.setBold(true);
  painter->setFont(f);
  painter->setPen(tc.legendText);
  painter->drawText(QRectF(8, 4, 90, 16), Qt::AlignLeft,
                    QStringLiteral("图例"));

  f.setBold(false);
  f.setPointSize(7);
  painter->setFont(f);

  struct Entry {
    QColor color;
    QString label;
  };
  Entry entries[] = {
      {tc.directionInput, QStringLiteral("输入")},
      {tc.directionOutput, QStringLiteral("输出")},
      {tc.directionBidirectional, QStringLiteral("双向")},
  };

  for (int i = 0; i < 3; ++i) {
    qreal y = 24 + i * 16;
    painter->setBrush(entries[i].color);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QPointF(14, y + 4), 4, 4);
    painter->setPen(tc.legendText);
    painter->drawText(QRectF(24, y, 70, 14), Qt::AlignLeft, entries[i].label);
  }
}

// ═══════════════════════════════════════════════════════════════
//  MonitorItem
// ═══════════════════════════════════════════════════════════════

MonitorItem::MonitorItem(int monitorIndex,
                         TopologyDocument* doc,
                         QGraphicsItem* parent)
    : TopologyBlockItem(doc, kWidth, kCornerRadius, parent),
      monitor_index_(monitorIndex) {
  setFlag(ItemIsSelectable);
  setAcceptHoverEvents(true);
  layoutPort();
}

void MonitorItem::layoutPort() {
  if (port_)
    return;
  port_ = new MonitorPortItem(monitor_index_, doc_, this);
  qreal h = effectiveHeight();
  port_->setPos(-kPortRadius, h / 2.0);
  addChildPort(port_);
}

QColor MonitorItem::blockFillColor() const {
  QColor c = topologyColors().monitorFill;
  c.setAlpha(160);
  return c;
}

QColor MonitorItem::blockBorderColor() const {
  return topologyColors().monitorBorder;
}

QPen MonitorItem::blockBorderPen(qreal penWidth) const {
  return QPen(blockBorderColor(), penWidth, Qt::DashLine);
}

void MonitorItem::paintContent(QPainter* painter,
                                const QStyleOptionGraphicsItem*,
                                const QRectF& rect) {
  const auto* mon = doc_->monitor(monitor_index_);
  if (!mon)
    return;

  const auto& tc = topologyColors();

  // Waveform icon in top-left corner
  qreal wx = rect.x() + 8;
  qreal wy = rect.y() + 10;
  painter->setPen(QPen(tc.textSecondary, 1.2));
  QPainterPath wave;
  wave.moveTo(wx, wy + 8);
  wave.cubicTo(wx + 4, wy, wx + 6, wy + 14, wx + 10, wy + 6);
  wave.cubicTo(wx + 12, wy + 2, wx + 14, wy + 10, wx + 16, wy + 6);
  painter->drawPath(wave);

  // Name
  painter->setPen(tc.textPrimary);
  QFont f = painter->font();
  f.setPointSize(9);
  f.setBold(true);
  painter->setFont(f);
  QRectF nameRect(rect.x() + 22, rect.y() + 4, rect.width() - 26, 20);
  painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, mon->name);

  // Device type
  f.setPointSize(7);
  f.setBold(false);
  painter->setFont(f);
  painter->setPen(tc.textSecondary);
  QRectF typeRect(rect.x() + 22, rect.y() + 22, rect.width() - 26, 16);
  painter->drawText(typeRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QStringLiteral("[%1]").arg(mon->deviceType));

  // Tap dot matrix at bottom
  int tapCount = mon->taps.size();
  if (tapCount > 0) {
    qreal dotY = rect.bottom() - 10;
    qreal dotSpacing = 14.0;
    qreal totalW = qMin(tapCount, 12) * dotSpacing;
    qreal startX = rect.center().x() - totalW / 2.0;
    int maxDots = qMin(tapCount, 12);
    for (int i = 0; i < maxDots; ++i) {
      QColor dotColor = tc.directionBidirectional;
      qreal dx = startX + i * dotSpacing + 4;
      painter->setBrush(dotColor);
      painter->setPen(Qt::NoPen);
      painter->drawEllipse(QPointF(dx, dotY), 3, 3);
    }
  }
}

qreal MonitorItem::calcContentHeight() const {
  return kBaseHeight;
}

int MonitorItem::tapCount() const {
  const auto* mon = doc_->monitor(monitor_index_);
  return mon ? mon->taps.size() : 0;
}

void MonitorItem::onResizeFinished(const QSizeF&, const QPointF& oldPos) {
  auto* mon = doc_->monitor(monitor_index_);
  if (!mon)
    return;

  QSizeF oldSize = mon->size;
  QSizeF newSize(block_width_, block_height_);
  if (oldSize == newSize)
    return;

  mon->size = newSize;

  int idx = monitor_index_;
  QPointF newPos = pos();
  QPointer<TopologyDocument> doc = doc_;
  QTimer::singleShot(0, [doc, idx, oldSize, newSize, oldPos, newPos]() {
    if (!doc)
      return;
    doc->undoStack()->push(new ResizeItemCommand(
        doc, idx, ResizeItemCommand::Monitor, oldSize, newSize, oldPos, newPos));
  });
}

// ═══════════════════════════════════════════════════════════════
//  MonitorPortItem ── drag anchor on monitor left edge
// ═══════════════════════════════════════════════════════════════

MonitorPortItem::MonitorPortItem(int monitorIndex,
                                 TopologyDocument* doc,
                                 MonitorItem* parent)
    : AbstractPortItem(0, doc, parent),
      monitor_index_(monitorIndex) {
}

QRectF MonitorPortItem::boundingRect() const {
  return QRectF(-kRadius - 4, -kRadius - 4, (kRadius + 4) * 2,
                (kRadius + 4) * 2);
}

QPainterPath MonitorPortItem::shape() const {
  QPainterPath p;
  p.addEllipse(-kRadius - 2, -kRadius - 2, (kRadius + 2) * 2,
               (kRadius + 2) * 2);
  return p;
}

void MonitorPortItem::paint(QPainter* painter,
                             const QStyleOptionGraphicsItem* option,
                             QWidget*) {
  Q_UNUSED(option);
  painter->setRenderHint(QPainter::Antialiasing);
  const auto& tc = topologyColors();
  painter->setBrush(tc.monitorFill);
  painter->setPen(QPen(tc.monitorBorder, 1.5));
  painter->drawEllipse(QPointF(0, 0), kRadius, kRadius);
}

QPointF MonitorPortItem::sceneCenter() const {
  return mapToScene(QPointF(0, 0));
}

}  // namespace etest::topology
