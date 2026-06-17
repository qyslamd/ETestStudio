#include "TopologyBlockItem.h"
#include "TopologyScene.h"
#include "TopologyTheme.h"

#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace etest::topology {

static constexpr qreal kHandleSize = 8.0;

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
  qreal h = effectiveHeight();
  return QRectF(-m, -m, block_width_ + m * 2, h + m * 2);
}

QPainterPath TopologyBlockItem::shape() const {
  QPainterPath p;
  p.addRoundedRect(0, 0, block_width_, effectiveHeight(), corner_radius_,
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

  qreal h = effectiveHeight();
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
  painter->setPen(blockBorderPen(penWidth));
  painter->drawRoundedRect(QRectF(0, 0, block_width_, h), corner_radius_,
                           corner_radius_);

  // Delegate content to subclass
  paintContent(painter, option, QRectF(0, 0, block_width_, h));

  // Resize handles when selected
  if (option->state & QStyle::State_Selected) {
    painter->setBrush(tc.resizeHandleFill);
    painter->setPen(QPen(tc.resizeHandleBorder, 1.0));
    for (int i = 1; i <= 8; ++i) {
      painter->drawRect(
          resizeHandleRect(static_cast<ResizeHandle>(i)));
    }
  }
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
  // Resize handle cursor feedback — only when selected
  if (active_handle_ == ResizeHandle::None && isSelected()) {
    ResizeHandle h = handleAt(event->pos());
    if (h != ResizeHandle::None) {
      updateCursorForHandle(h);
      if (body_hovered_) {
        body_hovered_ = false;
        update();
      }
      return;
    }
    // Not on a handle → restore default (unless over a child port)
    if (!isOverChildPort(event->pos()))
      setCursor(Qt::SizeAllCursor);
  }

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
  setCursor(Qt::SizeAllCursor);
}

// ═══════════════════════════════════════════════════════════════
//  Resize support
// ═══════════════════════════════════════════════════════════════

void TopologyBlockItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton && isSelected()) {
    ResizeHandle h = handleAt(event->pos());
    if (h != ResizeHandle::None) {
      active_handle_ = h;
      resize_start_pos_ = event->pos();
      old_pos_on_press_ = pos();
      old_width_on_press_ = block_width_;
      old_height_on_press_ = block_height_;
      setFlag(ItemIsMovable, false);
      event->accept();
      return;
    }
  }
  QGraphicsItem::mousePressEvent(event);
}

void TopologyBlockItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if (active_handle_ != ResizeHandle::None) {
    QPointF delta = event->pos() - resize_start_pos_;
    doResize(delta);
    resize_start_pos_ = event->pos();
    event->accept();
    return;
  }
  QGraphicsItem::mouseMoveEvent(event);
}

void TopologyBlockItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (active_handle_ != ResizeHandle::None) {
    active_handle_ = ResizeHandle::None;
    setFlag(ItemIsMovable, true);
    setCursor(Qt::SizeAllCursor);

    QSizeF oldSize(old_width_on_press_, old_height_on_press_);
    onResizeFinished(oldSize, old_pos_on_press_);

    event->accept();
    return;
  }
  QGraphicsItem::mouseReleaseEvent(event);
}

QRectF TopologyBlockItem::resizeHandleRect(ResizeHandle h) const {
  qreal hs = kHandleSize / 2.0;
  qreal w = block_width_;
  qreal hh = effectiveHeight();

  switch (h) {
    case ResizeHandle::TopLeft:
      return QRectF(-hs, -hs, kHandleSize, kHandleSize);
    case ResizeHandle::TopMid:
      return QRectF(w / 2.0 - hs, -hs, kHandleSize, kHandleSize);
    case ResizeHandle::TopRight:
      return QRectF(w - hs, -hs, kHandleSize, kHandleSize);
    case ResizeHandle::RightMid:
      return QRectF(w - hs, hh / 2.0 - hs, kHandleSize, kHandleSize);
    case ResizeHandle::BottomRight:
      return QRectF(w - hs, hh - hs, kHandleSize, kHandleSize);
    case ResizeHandle::BottomMid:
      return QRectF(w / 2.0 - hs, hh - hs, kHandleSize, kHandleSize);
    case ResizeHandle::BottomLeft:
      return QRectF(-hs, hh - hs, kHandleSize, kHandleSize);
    case ResizeHandle::LeftMid:
      return QRectF(-hs, hh / 2.0 - hs, kHandleSize, kHandleSize);
    default:
      return QRectF();
  }
}

TopologyBlockItem::ResizeHandle TopologyBlockItem::handleAt(
    const QPointF& pos) const {
  for (int i = 1; i <= 8; ++i) {
    auto h = static_cast<ResizeHandle>(i);
    if (resizeHandleRect(h).contains(pos))
      return h;
  }
  return ResizeHandle::None;
}

void TopologyBlockItem::updateCursorForHandle(ResizeHandle h) {
  switch (h) {
    case ResizeHandle::TopLeft:
    case ResizeHandle::BottomRight:
      setCursor(Qt::SizeFDiagCursor);
      break;
    case ResizeHandle::TopRight:
    case ResizeHandle::BottomLeft:
      setCursor(Qt::SizeBDiagCursor);
      break;
    case ResizeHandle::TopMid:
    case ResizeHandle::BottomMid:
      setCursor(Qt::SizeVerCursor);
      break;
    case ResizeHandle::LeftMid:
    case ResizeHandle::RightMid:
      setCursor(Qt::SizeHorCursor);
      break;
    default:
      setCursor(Qt::SizeAllCursor);
      break;
  }
}

void TopologyBlockItem::doResize(const QPointF& delta) {
  prepareGeometryChange();

  const qreal minW = 60.0;
  const qreal minH = calcContentHeight();

  qreal oldW = block_width_;
  qreal oldH = effectiveHeight();
  qreal newW = oldW;
  qreal newH = oldH;
  QPointF newPos = pos();

  switch (active_handle_) {
    case ResizeHandle::RightMid:
      newW = qMax(minW, oldW + delta.x());
      break;
    case ResizeHandle::LeftMid:
      newW = qMax(minW, oldW - delta.x());
      newPos.rx() += oldW - newW;
      break;
    case ResizeHandle::BottomMid:
      newH = qMax(minH, oldH + delta.y());
      break;
    case ResizeHandle::TopMid:
      newH = qMax(minH, oldH - delta.y());
      newPos.ry() += oldH - newH;
      break;
    case ResizeHandle::TopLeft:
      newW = qMax(minW, oldW - delta.x());
      newH = qMax(minH, oldH - delta.y());
      newPos.rx() += oldW - newW;
      newPos.ry() += oldH - newH;
      break;
    case ResizeHandle::TopRight:
      newW = qMax(minW, oldW + delta.x());
      newH = qMax(minH, oldH - delta.y());
      newPos.ry() += oldH - newH;
      break;
    case ResizeHandle::BottomLeft:
      newW = qMax(minW, oldW - delta.x());
      newH = qMax(minH, oldH + delta.y());
      newPos.rx() += oldW - newW;
      break;
    case ResizeHandle::BottomRight:
      newW = qMax(minW, oldW + delta.x());
      newH = qMax(minH, oldH + delta.y());
      break;
    default:
      break;
  }

  // Apply width change
  block_width_ = newW;

  // Apply height change — store in block_height_ if > auto minimum
  if (newH > calcContentHeight()) {
    block_height_ = newH;
  } else {
    block_height_ = 0;  // back to auto
  }

  if (newPos != pos())
    setPos(newPos);

  // Notify scene so connection paths update to new port positions
  if (auto* s = qobject_cast<TopologyScene*>(scene()))
    s->onItemMoved();

  update();
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
