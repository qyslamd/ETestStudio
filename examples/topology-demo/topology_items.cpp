#include "topology_items.h"
#include "TopologyDocument.h"
#include "TopologyScene.h"

#include <QCursor>
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>

namespace topology {

constexpr qreal kLineLength = 28.0;
constexpr qreal kEndRadius = 3.0;

// ═══════════════════════════════════════════════════════════════
//  PortItem
// ═══════════════════════════════════════════════════════════════

PortItem::PortItem(int productIndex, int portIndex, TopologyDocument* doc,
                   UutItem* parent)
    : QGraphicsItem(parent), product_index_(productIndex),
      port_index_(portIndex), doc_(doc) {
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
    return p;
}

void PortItem::paint(QPainter* painter,
                     const QStyleOptionGraphicsItem* option, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);

    const auto* prod = doc_->product(product_index_);
    if (!prod || port_index_ >= prod->ports.size()) return;
    const auto& port = prod->ports[port_index_];

    QColor color = (port.direction == TopologyPort::Input)
                       ? QColor(66, 133, 244)
                       : QColor(52, 168, 83);

    if (option->state & QStyle::State_Selected) {
        color = color.lighter(130);
    }

    bool isInput = (port.direction == TopologyPort::Input);
    int sign = isInput ? -1 : 1;
    qreal lineEndX = sign * kLineLength;

    painter->setPen(QPen(color.darker(130), 1.5));
    painter->drawLine(QPointF(0, 0), QPointF(lineEndX, 0));

    painter->setBrush(color);
    painter->drawEllipse(QPointF(lineEndX, 0), kEndRadius, kEndRadius);

    painter->setBrush(color);
    painter->setPen(QPen(color.darker(130), 1.5));
    painter->drawEllipse(QPointF(0, 0), kRadius, kRadius);

    painter->setPen(Qt::black);
    QFont f = painter->font();
    f.setPointSize(7);
    painter->setFont(f);

    QString label = port.name;
    qreal tw = painter->fontMetrics().horizontalAdvance(label);
    qreal midX = lineEndX / 2.0;
    painter->drawText(QPointF(midX - tw / 2.0, -8), label);
}

QPointF PortItem::sceneCenter() const {
    return mapToScene(QPointF(0, 0));
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

UutItem::UutItem(int productIndex, TopologyDocument* doc,
                 QGraphicsItem* parent)
    : QGraphicsItem(parent), product_index_(productIndex), doc_(doc) {
    setFlag(ItemIsMovable);
    setFlag(ItemIsSelectable);
    setFlag(ItemSendsGeometryChanges);
    setCursor(Qt::SizeAllCursor);

    layoutPorts();
}

QRectF UutItem::boundingRect() const {
    qreal m = 2.0;
    return QRectF(-m, -m, kWidth + m * 2, kHeight + m * 2);
}

void UutItem::paint(QPainter* painter,
                    const QStyleOptionGraphicsItem* option, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);

    QColor fill(189, 215, 238);
    if (option->state & QStyle::State_Selected) fill = fill.lighter(120);
    painter->setBrush(fill);
    painter->setPen(QPen(QColor(66, 133, 244), 1.5));
    painter->drawRect(QRectF(0, 0, kWidth, kHeight));

    const auto* prod = doc_->product(product_index_);
    if (!prod) return;

    painter->setPen(Qt::black);
    QFont f = painter->font();
    f.setPointSize(10);
    f.setBold(true);
    painter->setFont(f);
    painter->drawText(QRectF(0, 0, kWidth, kHeight), Qt::AlignCenter,
                      prod->name);
}

void UutItem::layoutPorts() {
    ports_.clear();
    const auto* prod = doc_->product(product_index_);
    if (!prod) return;

    int inputCount = 0, outputCount = 0;
    for (const auto& p : prod->ports) {
        if (p.direction == TopologyPort::Input)
            ++inputCount;
        else
            ++outputCount;
    }

    int inputIdx = 0, outputIdx = 0;
    qreal topMargin = 10.0;
    qreal bottomMargin = 10.0;

    for (int i = 0; i < prod->ports.size(); ++i) {
        auto* portItem = new PortItem(product_index_, i, doc_, this);
        ports_.append(portItem);

        const auto& p = prod->ports[i];
        qreal y;
        bool isInput = (p.direction == TopologyPort::Input);
        int count = isInput ? inputCount : outputCount;
        int& idx = isInput ? inputIdx : outputIdx;

        if (count <= 1) {
            y = kHeight / 2.0;
        } else {
            qreal spacing = (kHeight - topMargin - bottomMargin) / (count - 1);
            y = topMargin + idx * spacing;
        }

        if (isInput) {
            portItem->setPos(0, y);
        } else {
            portItem->setPos(kWidth, y);
        }
        ++idx;
    }
}

PortItem* UutItem::portItem(int portIndex) const {
    if (portIndex < 0 || portIndex >= ports_.size()) return nullptr;
    return ports_[portIndex];
}

QPointF UutItem::portScenePos(int portIndex) const {
    auto* pi = portItem(portIndex);
    return pi ? pi->sceneCenter() : QPointF();
}

QVariant UutItem::itemChange(GraphicsItemChange change,
                             const QVariant& value) {
    if (change == ItemPositionHasChanged) {
        if (auto* s = qobject_cast<TopologyScene*>(scene())) {
            s->onItemMoved();
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

// ═══════════════════════════════════════════════════════════════
//  DeviceItem
// ═══════════════════════════════════════════════════════════════

DeviceItem::DeviceItem(int deviceIndex, TopologyDocument* doc,
                       QGraphicsItem* parent)
    : QGraphicsItem(parent), device_index_(deviceIndex), doc_(doc) {
    setFlag(ItemIsMovable);
    setFlag(ItemIsSelectable);
    setFlag(ItemSendsGeometryChanges);
    setCursor(Qt::SizeAllCursor);
    layoutDevicePorts();
}

QRectF DeviceItem::boundingRect() const {
    return QRectF(0, 0, kWidth, contentHeight());
}

void DeviceItem::paint(QPainter* painter,
                       const QStyleOptionGraphicsItem* option, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);

    qreal h = contentHeight();
    QColor fill(255, 228, 181);
    if (option->state & QStyle::State_Selected) fill = fill.lighter(120);
    painter->setBrush(fill);
    painter->setPen(QPen(QColor(230, 145, 56), 1.5));
    painter->drawRoundedRect(QRectF(0, 0, kWidth, h), 6, 6);

    const auto* dev = doc_->device(device_index_);
    if (!dev) return;

    painter->setPen(Qt::black);
    QFont f = painter->font();
    f.setPointSize(9);
    f.setBold(true);
    painter->setFont(f);
    painter->drawText(QRectF(0, 6, kWidth, 20), Qt::AlignCenter, dev->name);

    f.setPointSize(7);
    f.setBold(false);
    painter->setFont(f);
    painter->setPen(QColor(100, 100, 100));
    painter->drawText(QRectF(0, 26, kWidth, 18), Qt::AlignCenter,
                      QStringLiteral("[%1]").arg(dev->deviceType));
}

QString DeviceItem::deviceType() const {
    const auto* dev = doc_->device(device_index_);
    return dev ? dev->deviceType : QString();
}

QPointF DeviceItem::connectionPoint() const {
    return mapToScene(QPointF(kWidth, contentHeight() / 2.0));
}

qreal DeviceItem::contentHeight() const {
    const auto* dev = doc_->device(device_index_);
    int portCount = dev ? dev->ports.size() : 0;
    constexpr qreal kPortSpacing = 20.0;
    return qMax(kBaseHeight, 2 * kPortMargin + portCount * kPortSpacing);
}

void DeviceItem::layoutDevicePorts() {
    for (auto* p : device_port_items_) {
        if (scene()) scene()->removeItem(p);
        delete p;
    }
    device_port_items_.clear();

    const auto* dev = doc_->device(device_index_);
    if (!dev) return;

    qreal h = contentHeight();
    int n = dev->ports.size();
    for (int i = 0; i < n; ++i) {
        auto* portItem = new DevicePortItem(device_index_, i, doc_, this);
        device_port_items_.append(portItem);

        qreal y;
        if (n <= 1) {
            y = h / 2.0;
        } else {
            y = kPortMargin + i * (h - 2 * kPortMargin) / (n - 1);
        }
        portItem->setPos(kWidth, y);
    }
}

DevicePortItem* DeviceItem::devicePortItem(int portIndex) const {
    if (portIndex < 0 || portIndex >= device_port_items_.size())
        return nullptr;
    return device_port_items_[portIndex];
}

QVariant DeviceItem::itemChange(GraphicsItemChange change,
                                const QVariant& value) {
    if (change == ItemPositionHasChanged) {
        if (auto* s = qobject_cast<TopologyScene*>(scene())) {
            s->onItemMoved();
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

// ═══════════════════════════════════════════════════════════════
//  DevicePortItem
// ═══════════════════════════════════════════════════════════════

DevicePortItem::DevicePortItem(int deviceIndex, int portIndex,
                               TopologyDocument* doc, DeviceItem* parent)
    : QGraphicsItem(parent), device_index_(deviceIndex),
      port_index_(portIndex), doc_(doc) {
    setFlag(ItemIsSelectable);
    setCursor(Qt::PointingHandCursor);
}

QRectF DevicePortItem::boundingRect() const {
    return QRectF(-kRadius - 4, -16,
                  kLineLength + kEndRadius + kRadius + 80 + 4, 30);
}

QPainterPath DevicePortItem::shape() const {
    QPainterPath p;
    p.addEllipse(-kRadius, -kRadius, kRadius * 2, kRadius * 2);
    return p;
}

void DevicePortItem::paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);

    const auto* dev = doc_->device(device_index_);
    if (!dev || port_index_ >= dev->ports.size()) return;
    const auto& port = dev->ports[port_index_];

    QColor color(180, 180, 180);
    if (option->state & QStyle::State_Selected) {
        color = color.lighter(130);
    }

    qreal lineEndX = kLineLength;

    painter->setPen(QPen(color.darker(130), 1.5));
    painter->drawLine(QPointF(0, 0), QPointF(lineEndX, 0));

    painter->setBrush(color);
    painter->drawEllipse(QPointF(lineEndX, 0), kEndRadius, kEndRadius);

    painter->setBrush(color);
    painter->setPen(QPen(color.darker(130), 1.5));
    painter->drawEllipse(QPointF(0, 0), kRadius, kRadius);

    painter->setPen(Qt::black);
    QFont f = painter->font();
    f.setPointSize(7);
    painter->setFont(f);
    QString label = QStringLiteral("%1 [%2]")
                        .arg(port.name, functionTypeToString(port.functionType));
    qreal tw = painter->fontMetrics().horizontalAdvance(label);
    qreal midX = lineEndX / 2.0;
    painter->drawText(QPointF(midX - tw / 2.0, -8), label);
}

DeviceItem* DevicePortItem::parentDeviceItem() const {
    return qgraphicsitem_cast<DeviceItem*>(parentItem());
}

QPointF DevicePortItem::sceneCenter() const {
    return mapToScene(QPointF(0, 0));
}

// ═══════════════════════════════════════════════════════════════
//  ConnectionItem
// ═══════════════════════════════════════════════════════════════

ConnectionItem::ConnectionItem(PortItem* source, DevicePortItem* target,
                               const QString& devicePort,
                               QGraphicsItem* parent)
    : QGraphicsPathItem(parent), source_(source), target_port_(target),
      device_port_(devicePort) {
    setFlag(ItemIsSelectable);
    setZValue(-1);
}

ConnectionItem::~ConnectionItem() {}

void ConnectionItem::updatePath() {
    if (!source_ || !target_port_) return;

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
        cp1 = start + QPointF(cpOffset, 0);
        cp2 = end + QPointF(cpOffset, 0);
    }
    p.cubicTo(cp1, cp2, end);
    setPath(p);
}

DeviceItem* ConnectionItem::targetDevice() const {
    return target_port_ ? target_port_->parentDeviceItem() : nullptr;
}

}  // namespace topology
