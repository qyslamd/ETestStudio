#include "TopologyBlockItem.h"
#include "TopologyScene.h"
#include "TopologyTheme.h"

#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace etest::topology {

TopologyBlockItem::TopologyBlockItem(TopologyDocument* doc,
                                     qreal width,
                                     qreal cornerRadius,
                                     QGraphicsItem* parent)
    : QGraphicsItem(parent),
      doc_(doc),
      block_width_(width),
      corner_radius_(cornerRadius) {
  setFlag(ItemIsMovable);
  setFlag(ItemIsSelectable);
  setFlag(ItemSendsGeometryChanges);
  setAcceptHoverEvents(true);
  setCursor(Qt::SizeAllCursor);
}

QRectF TopologyBlockItem::boundingRect() const {
  qreal m = 6.0;
  qreal h = calcContentHeight();
  return QRectF(-m, -m, block_width_ + m * 2, h + m * 2);
}

QPainterPath TopologyBlockItem::shape() const {
  QPainterPath p;
  p.addRoundedRect(0, 0, block_width_, calcContentHeight(), corner_radius_,
                   corner_radius_);
  return p;
}

bool TopologyBlockItem::contains(const QPointF& point) const {
  if (!QGraphicsItem::contains(point))
    return false;
  if (isOverChildPort(point))
    return false;
  return true;
}

void TopologyBlockItem::paint(QPainter* painter,
                               const QStyleOptionGraphicsItem* option,
                               QWidget*) {
  painter->setRenderHint(QPainter::Antialiasing);

  qreal h = calcContentHeight();
  const auto& tc = topologyColors();

  QColor fill = blockFillColor();
  QColor border = blockBorderColor();
  qreal penWidth = 1.5;

  if (body_hovered_ && !hasChildHovered()) {
    fill = fill.darker(115);
    border = border.darker(120);
  }
  if (option->state & QStyle::State_Selected) {
    fill = fill.lighter(130);
    penWidth = 2.5;
  }

  // Two-layer shadow
  painter->setBrush(tc.shadowDark);
  painter->setPen(Qt::NoPen);
  painter->drawRoundedRect(QRectF(4, 4, block_width_, h), corner_radius_,
                           corner_radius_);
  painter->setBrush(tc.shadowLight);
  painter->drawRoundedRect(QRectF(2, 2, block_width_, h), corner_radius_,
                           corner_radius_);

  // Body
  painter->setBrush(fill);
  painter->setPen(QPen(border, penWidth));
  painter->drawRoundedRect(QRectF(0, 0, block_width_, h), corner_radius_,
                           corner_radius_);

  // Delegate content to subclass
  paintContent(painter, option, QRectF(0, 0, block_width_, h));
}

QVariant TopologyBlockItem::itemChange(GraphicsItemChange change,
                                       const QVariant& value) {
  if (change == ItemPositionHasChanged) {
    if (auto* s = qobject_cast<TopologyScene*>(scene())) {
      s->onItemMoved();
    }
  }
  return QGraphicsItem::itemChange(change, value);
}

void TopologyBlockItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
  if (isOverChildPort(event->pos()))
    return;
  body_hovered_ = true;
  update();
}

void TopologyBlockItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event) {
  bool overPort = isOverChildPort(event->pos());
  if (overPort && body_hovered_) {
    body_hovered_ = false;
    update();
  } else if (!overPort && !body_hovered_) {
    body_hovered_ = true;
    update();
  }
}

void TopologyBlockItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*) {
  if (body_hovered_) {
    body_hovered_ = false;
    update();
  }
}

void TopologyBlockItem::addChildPort(QGraphicsItem* port) {
  child_ports_.append(port);
}

void TopologyBlockItem::clearChildPorts() {
  child_ports_.clear();
}

bool TopologyBlockItem::isOverChildPort(const QPointF& point) const {
  for (auto* port : child_ports_) {
    if (port->isVisible() && port->shape().contains(port->mapFromParent(point)))
      return true;
  }
  return false;
}

}  // namespace etest::topology
