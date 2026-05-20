#define _USE_MATH_DEFINES
#include <cmath>

#include "TopologyDocument.h"
#include "TopologyScene.h"
#include "TopologyTheme.h"
#include "topology_items.h"

#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>

namespace etest::topology {

constexpr qreal kLineLength = 28.0;
constexpr qreal kEndRadius = 3.0;
constexpr qreal kPortRadius = 6.0;

// ═══════════════════════════════════════════════════════════════
//  PortItem
// ═══════════════════════════════════════════════════════════════

PortItem::PortItem(int productIndex,
                   int portIndex,
                   TopologyDocument* doc,
                   UutItem* parent)
    : QGraphicsItem(parent),
      product_index_(productIndex),
      port_index_(portIndex),
      doc_(doc) {
  setAcceptHoverEvents(true);
  setCursor(Qt::CrossCursor);
  setFlag(ItemIsSelectable);
}

QRectF PortItem::boundingRect() const {
  return QRectF(-kLineLength - kEndRadius - 80, -16,
                kLineLength * 2 + kEndRadius * 2 + 80 * 2, 30);
}

QPainterPath PortItem::shape() const {
  QPainterPath p;
  p.addEllipse(-kRadius, -kRadius, kRadius * 2, kRadius * 2);
  p.addEllipse(-kLineLength - kEndRadius, -kEndRadius, kEndRadius * 2,
               kEndRadius * 2);
  QPainterPath line;
  line.moveTo(0, 0);
  line.lineTo(-kLineLength, 0);
  QPainterPathStroker stroker;
  stroker.setWidth(8.0);
  p.addPath(stroker.createStroke(line));
  return p;
}

static QColor directionColor(TopologyPort::Direction d) {
  const auto& tc = topologyColors();
  switch (d) {
    case TopologyPort::Input:
      return tc.directionInput;
    case TopologyPort::Output:
      return tc.directionOutput;
    case TopologyPort::Bidirectional:
      return tc.directionBidirectional;
  }
  return QColor(128, 128, 128);
}

void PortItem::hoverEnterEvent(QGraphicsSceneHoverEvent*) {
  hovered_ = true;
  update();
}

void PortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*) {
  hovered_ = false;
  update();
}

void PortItem::paint(QPainter* painter,
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

  painter->setPen(QPen(color, penWidth));
  painter->drawLine(QPointF(0, 0), QPointF(lineEndX, 0));

  painter->setBrush(color);
  painter->setPen(Qt::NoPen);
  painter->drawEllipse(QPointF(lineEndX, 0), kEndRadius + 0.5,
                       kEndRadius + 0.5);

  painter->setBrush(color);
  painter->setPen(QPen(color.darker(140), penWidth));
  painter->drawEllipse(QPointF(0, 0), dotRadius, dotRadius);

  if (port.direction == TopologyPort::Bidirectional) {
    qreal as = 4.0;
    qreal gap = 3.0;
    qreal ex = lineEndX;
    painter->setPen(QPen(color, penWidth));
    painter->drawLine(QPointF(ex - gap - as, 0), QPointF(ex - gap, -as));
    painter->drawLine(QPointF(ex - gap - as, 0), QPointF(ex - gap, as));
    painter->drawLine(QPointF(ex + gap + as, 0), QPointF(ex + gap, -as));
    painter->drawLine(QPointF(ex + gap + as, 0), QPointF(ex + gap, as));
    painter->drawLine(QPointF(ex - gap, 0), QPointF(ex + gap, 0));
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

QPointF PortItem::sceneCenter() const {
  return mapToScene(QPointF(-kLineLength, 0));
}

void PortItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  QGraphicsItem::mousePressEvent(event);
  press_pos_ = event->scenePos();
  event->accept();
}

void PortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if ((event->scenePos() - press_pos_).manhattanLength() > 5) {
    if (auto* s = qobject_cast<TopologyScene*>(scene())) {
      s->startConnectionDrag(this, press_pos_);
      s->continueConnectionDrag(event->scenePos());
    }
  }
  event->accept();
}

void PortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (auto* s = qobject_cast<TopologyScene*>(scene())) {
    s->finishConnectionDrag(event->scenePos());
  }
  event->accept();
}

// ═══════════════════════════════════════════════════════════════
//  UutItem
// ═══════════════════════════════════════════════════════════════

UutItem::UutItem(int productIndex, TopologyDocument* doc, QGraphicsItem* parent)
    : TopologyBlockItem(doc, kWidth, kCornerRadius, parent),
      product_index_(productIndex) {
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

  qreal h = calcContentHeight();
  int n = prod->ports.size();
  for (int i = 0; i < n; ++i) {
    auto* portItem = new PortItem(product_index_, i, doc_, this);
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

PortItem* UutItem::portItem(int portIndex) const {
  if (portIndex < 0 || portIndex >= ports_.size())
    return nullptr;
  return ports_[portIndex];
}

QPointF UutItem::portScenePos(int portIndex) const {
  auto* pi = portItem(portIndex);
  return pi ? pi->sceneCenter() : QPointF();
}

// ═══════════════════════════════════════════════════════════════
//  DeviceItem
// ═══════════════════════════════════════════════════════════════

DeviceItem::DeviceItem(int deviceIndex,
                       TopologyDocument* doc,
                       QGraphicsItem* parent)
    : TopologyBlockItem(doc, kWidth, kCornerRadius, parent),
      device_index_(deviceIndex) {
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
  return mapToScene(
      QPointF(block_width_, calcContentHeight() / 2.0));
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

  qreal h = calcContentHeight();
  int n = dev->ports.size();
  for (int i = 0; i < n; ++i) {
    auto* portItem = new DevicePortItem(device_index_, i, doc_, this);
    device_port_items_.append(portItem);
    addChildPort(portItem);

    qreal y;
    if (n <= 1) {
      y = h / 2.0;
    } else {
      y = kPortMargin + i * (h - 2 * kPortMargin) / (n - 1);
    }
    portItem->setPos(kWidth + kPortRadius, y);
  }
}

DevicePortItem* DeviceItem::devicePortItem(int portIndex) const {
  if (portIndex < 0 || portIndex >= device_port_items_.size())
    return nullptr;
  return device_port_items_[portIndex];
}

// ═══════════════════════════════════════════════════════════════
//  DevicePortItem
// ═══════════════════════════════════════════════════════════════

DevicePortItem::DevicePortItem(int deviceIndex,
                               int portIndex,
                               TopologyDocument* doc,
                               DeviceItem* parent)
    : QGraphicsItem(parent),
      device_index_(deviceIndex),
      port_index_(portIndex),
      doc_(doc) {
  setFlag(ItemIsSelectable);
  setAcceptHoverEvents(true);
  setCursor(Qt::CrossCursor);
}

QRectF DevicePortItem::boundingRect() const {
  return QRectF(-kRadius - 4, -16, kLineLength + kEndRadius + kRadius + 80 + 4,
                30);
}

QPainterPath DevicePortItem::shape() const {
  QPainterPath p;
  p.addEllipse(-kRadius, -kRadius, kRadius * 2, kRadius * 2);
  p.addEllipse(kLineLength - kEndRadius, -kEndRadius, kEndRadius * 2,
               kEndRadius * 2);
  QPainterPath line;
  line.moveTo(0, 0);
  line.lineTo(kLineLength, 0);
  QPainterPathStroker stroker;
  stroker.setWidth(8.0);
  p.addPath(stroker.createStroke(line));
  return p;
}

void DevicePortItem::hoverEnterEvent(QGraphicsSceneHoverEvent*) {
  hovered_ = true;
  update();
}

void DevicePortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*) {
  hovered_ = false;
  update();
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

  painter->setBrush(color);
  painter->setPen(Qt::NoPen);
  painter->drawEllipse(QPointF(lineEndX, 0), kEndRadius + 0.5,
                       kEndRadius + 0.5);

  painter->setBrush(color);
  painter->setPen(QPen(color.darker(140), penWidth));
  painter->drawEllipse(QPointF(0, 0), dotRadius, dotRadius);

  if (port.direction == TopologyPort::Bidirectional) {
    qreal as = 4.0;
    qreal gap = 3.0;
    qreal ex = lineEndX;
    painter->setPen(QPen(color, penWidth));
    painter->drawLine(QPointF(ex - gap - as, 0), QPointF(ex - gap, -as));
    painter->drawLine(QPointF(ex - gap - as, 0), QPointF(ex - gap, as));
    painter->drawLine(QPointF(ex + gap + as, 0), QPointF(ex + gap, -as));
    painter->drawLine(QPointF(ex + gap + as, 0), QPointF(ex + gap, as));
    painter->drawLine(QPointF(ex - gap, 0), QPointF(ex + gap, 0));
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

void DevicePortItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  QGraphicsItem::mousePressEvent(event);
  press_pos_ = event->scenePos();
  event->accept();
}

void DevicePortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if ((event->scenePos() - press_pos_).manhattanLength() > 5) {
    if (auto* s = qobject_cast<TopologyScene*>(scene())) {
      s->startConnectionDrag(this, press_pos_);
      s->continueConnectionDrag(event->scenePos());
    }
  }
  event->accept();
}

void DevicePortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (auto* s = qobject_cast<TopologyScene*>(scene())) {
    s->finishConnectionDrag(event->scenePos());
  }
  event->accept();
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

ConnectionItem::ConnectionItem(PortItem* source,
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

  QPainterPath p;
  p.moveTo(start);

  qreal dx = qAbs(end.x() - start.x());
  qreal cpOffset = qMax(dx * 0.5, 50.0);
  QPointF cp1, cp2;
  if (end.x() > start.x()) {
    cp1 = start + QPointF(cpOffset, 0);
    cp2 = end - QPointF(cpOffset, 0);
  } else {
    cp1 = start - QPointF(cpOffset, 0);
    cp2 = end + QPointF(cpOffset, 0);
  }
  p.cubicTo(cp1, cp2, end);
  setPath(p);

  // Compute direction arrow at midpoint
  qreal t = 0.5;
  QPointF mid = p.pointAtPercent(t);
  QPointF tang = p.pointAtPercent(t + 0.01) - p.pointAtPercent(t - 0.01);
  qreal angle = atan2(tang.y(), tang.x());

  const auto* prod = doc_->product(source_->productIndex());
  arrow_path_ = QPainterPath();
  if (!prod || source_->portIndex() >= prod->ports.size())
    return;
  auto dir = prod->ports[source_->portIndex()].direction;

  qreal as = 8.0;
  if (dir == TopologyPort::Bidirectional) {
    qreal gap = 8.0;
    // Forward triangle (points in path direction)
    qreal a = angle;
    QPointF fwdCenter = mid + QPointF(cos(a) * gap, sin(a) * gap);
    arrow_path_.moveTo(fwdCenter + QPointF(cos(a) * as, sin(a) * as));
    arrow_path_.lineTo(fwdCenter + QPointF(cos(a + 2.5) * as, sin(a + 2.5) * as));
    arrow_path_.lineTo(fwdCenter + QPointF(cos(a - 2.5) * as, sin(a - 2.5) * as));
    arrow_path_.closeSubpath();
    // Backward triangle (points opposite path direction)
    a = angle + M_PI;
    QPointF revCenter = mid + QPointF(cos(a) * gap, sin(a) * gap);
    arrow_path_.moveTo(revCenter + QPointF(cos(a) * as, sin(a) * as));
    arrow_path_.lineTo(revCenter + QPointF(cos(a + 2.5) * as, sin(a + 2.5) * as));
    arrow_path_.lineTo(revCenter + QPointF(cos(a - 2.5) * as, sin(a - 2.5) * as));
    arrow_path_.closeSubpath();
  } else {
    bool forward = (dir == TopologyPort::Input);
    qreal a = forward ? angle : angle + M_PI;
    arrow_path_.moveTo(mid + QPointF(cos(a) * as, sin(a) * as));
    arrow_path_.lineTo(mid + QPointF(cos(a + 2.5) * as, sin(a + 2.5) * as));
    arrow_path_.lineTo(mid + QPointF(cos(a - 2.5) * as, sin(a - 2.5) * as));
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
  const auto* prod = doc_->product(source_->productIndex());
  if (!prod || source_->portIndex() >= prod->ports.size())
    return;
  auto dir = prod->ports[source_->portIndex()].direction;

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

}  // namespace etest::topology
