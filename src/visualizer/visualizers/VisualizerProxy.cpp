#include "VisualizerProxy.h"

#include <QCursor>
#include <QColor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtGlobal>

namespace {
// 手柄配色（程序化绘制，后续可随主题 JSON 迁移到 QSS/palette）
const QColor kHandleColor(0x2E, 0x9E, 0xDF, 220);
const QColor kHandleBorder(0xFF, 0xFF, 0xFF, 200);
}  // namespace

namespace etest::visualizer {

VisualizerProxy::VisualizerProxy(QGraphicsItem* parent)
    : QGraphicsProxyWidget(parent) {
  setFlag(ItemIsMovable);
  setFlag(ItemIsSelectable);
  setFlag(ItemSendsGeometryChanges);
  setAcceptHoverEvents(true);
  setCursor(Qt::SizeAllCursor);
}

void VisualizerProxy::setEditMode(bool edit) {
  if (edit_mode_ == edit) {
    return;
  }
  edit_mode_ = edit;
  setFlag(ItemIsMovable, edit);
  setFlag(ItemIsSelectable, edit);
  active_handle_ = ResizeHandle::None;
  setCursor(edit ? Qt::SizeAllCursor : Qt::ArrowCursor);
  update();
}

QRectF VisualizerProxy::visualRect() const {
  const QSizeF s = widget() ? widget()->size() : QSizeF();
  return QRectF(pos(), s);
}

QVariant VisualizerProxy::itemChange(GraphicsItemChange change,
                                     const QVariant& value) {
  // 拖动会话内的位置变化记入 moved_，release 时据以决定是否发 geometryEdited
  if (change == ItemPositionHasChanged && drag_active_) {
    moved_ = true;
  }
  return QGraphicsProxyWidget::itemChange(change, value);
}

// ══════════════════════════════════════════════════════════════════════════════
// 绘制 / 几何
// ══════════════════════════════════════════════════════════════════════════════

QRectF VisualizerProxy::boundingRect() const {
  // 外扩 kHandleMargin，把 8 个手柄（中心压边缘、外扩半区）包进边界，
  // 否则手柄外半截超出命中区域、且取消选中后残留伪影
  return QGraphicsProxyWidget::boundingRect().adjusted(
      -kHandleMargin, -kHandleMargin, kHandleMargin, kHandleMargin);
}

void VisualizerProxy::paint(QPainter* painter,
                            const QStyleOptionGraphicsItem* option,
                            QWidget* widget) {
  QGraphicsProxyWidget::paint(painter, option, widget);

  // 编辑态 + 选中：画 8 方向 resize 手柄
  if (!edit_mode_ || !(option->state & QStyle::State_Selected)) {
    return;
  }
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setBrush(kHandleColor);
  painter->setPen(QPen(kHandleBorder, 1.0));
  for (int i = 1; i <= 8; ++i) {
    painter->drawRect(handleRect(static_cast<ResizeHandle>(i)));
  }
  painter->restore();
}

// ══════════════════════════════════════════════════════════════════════════════
// Resize 手柄（移植自拓扑 TopologyBlockItem）
// ══════════════════════════════════════════════════════════════════════════════

QRectF VisualizerProxy::handleRect(ResizeHandle h) const {
  const QSizeF s = widget() ? widget()->size() : QSizeF();
  const qreal hs = kHandleSize / 2.0;
  const qreal w = s.width();
  const qreal hh = s.height();

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

VisualizerProxy::ResizeHandle VisualizerProxy::handleAt(
    const QPointF& pos) const {
  for (int i = 1; i <= 8; ++i) {
    const auto h = static_cast<ResizeHandle>(i);
    if (handleRect(h).contains(pos)) {
      return h;
    }
  }
  return ResizeHandle::None;
}

void VisualizerProxy::updateCursorForHandle(ResizeHandle h) {
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

// ══════════════════════════════════════════════════════════════════════════════
// 鼠标交互
// ══════════════════════════════════════════════════════════════════════════════

void VisualizerProxy::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  if (edit_mode_ && event->button() == Qt::LeftButton) {
    drag_active_ = true;  // 拖动会话开始（移动或 resize，release 时结算置脏）
    // 编辑态：proxy 直接处理，不转发给内嵌 widget——widget（波形图等）会
    // 消费鼠标导致卡片选不中/拖不动。预览无数据，内部交互可牺牲。
    if (isSelected()) {
      const ResizeHandle h = handleAt(event->pos());
      if (h != ResizeHandle::None) {
        active_handle_ = h;
        resize_start_pos_ = event->pos();
        setFlag(ItemIsMovable, false);  // 拖动期间禁整体移动
        event->accept();
        return;
      }
    }
    // 不手动 setSelected：选中/多选（Ctrl toggle）由 QGraphicsScene 在分发
    // 前处理，这里只准备移动（ItemIsMovable），否则强制选中与 scene 冲突
    setFlag(ItemIsMovable, true);
    QGraphicsItem::mousePressEvent(event);  // 基类：ItemIsMovable 开始移动
    event->accept();
    return;
  }
  QGraphicsProxyWidget::mousePressEvent(event);
}

void VisualizerProxy::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if (edit_mode_) {
    if (active_handle_ != ResizeHandle::None) {
      const QPointF delta = event->pos() - resize_start_pos_;
      doResize(delta);
      resize_start_pos_ = event->pos();
      event->accept();
      return;
    }
    QGraphicsItem::mouseMoveEvent(event);  // 基类：移动 proxy
    event->accept();
    return;
  }
  QGraphicsProxyWidget::mouseMoveEvent(event);
}

void VisualizerProxy::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (edit_mode_) {
    if (active_handle_ != ResizeHandle::None) {
      // resize 会话结束：几何变化则通知置脏
      if (resized_) {
        emit geometryEdited();
      }
      active_handle_ = ResizeHandle::None;
      setFlag(ItemIsMovable, true);
      setCursor(Qt::SizeAllCursor);
      drag_active_ = false;
      moved_ = false;
      resized_ = false;
      event->accept();
      return;
    }
    QGraphicsItem::mouseReleaseEvent(event);
    // 移动会话结束：位置变化（itemChange 记入 moved_）则通知置脏
    if (drag_active_) {
      if (moved_) {
        emit geometryEdited();
      }
      drag_active_ = false;
      moved_ = false;
      resized_ = false;
    }
    event->accept();
    return;
  }
  QGraphicsProxyWidget::mouseReleaseEvent(event);
}

void VisualizerProxy::hoverMoveEvent(QGraphicsSceneHoverEvent* event) {
  if (edit_mode_) {
    if (isSelected() && active_handle_ == ResizeHandle::None) {
      const ResizeHandle h = handleAt(event->pos());
      if (h != ResizeHandle::None) {
        updateCursorForHandle(h);
        return;
      }
    }
    setCursor(Qt::SizeAllCursor);
    return;  // 编辑态不转发基类（避免 hover 进内嵌 widget）
  }
  QGraphicsProxyWidget::hoverMoveEvent(event);
}

void VisualizerProxy::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
  setCursor(edit_mode_ ? Qt::SizeAllCursor : Qt::ArrowCursor);
  QGraphicsProxyWidget::hoverLeaveEvent(event);
}

void VisualizerProxy::doResize(const QPointF& delta) {
  if (!widget()) {
    return;
  }
  const QSizeF s = widget()->size();
  const qreal oldW = s.width();
  const qreal oldH = s.height();
  const qreal minW = qMax<qreal>(200.0, widget()->minimumSizeHint().width());
  const qreal minH = qMax<qreal>(120.0, widget()->minimumSizeHint().height());
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

  if (newPos != pos()) {
    setPos(newPos);  // 四角手柄保持对边固定
  }
  if (widget()->size() != QSizeF(newW, newH)) {
    widget()->resize(newW, newH);
    resized_ = true;
    update();
  }
}

}  // namespace etest::visualizer
