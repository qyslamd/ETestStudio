#include "TopologyView.h"
#include "TopologyScene.h"
#include "TopologyTheme.h"
#include "topology_items.h"

#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

namespace etest::topology {

TopologyView::TopologyView(TopologyScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent) {
  setRenderHint(QPainter::Antialiasing);
  setDragMode(QGraphicsView::RubberBandDrag);
  setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  // Allow zooming into empty areas
  setInteractive(true);

  // Style — adapt to current theme
  setBackgroundBrush(topologyColors().sceneBackground);
  setFrameShape(QFrame::NoFrame);
}

void TopologyView::drawForeground(QPainter* painter, const QRectF& rect) {
  Q_UNUSED(rect);
  painter->save();
  painter->resetTransform();

  int lw = 100, lh = 75;
  int x = viewport()->width() - lw - 10;
  int y = viewport()->height() - lh - 10;

  painter->setRenderHint(QPainter::Antialiasing);
  const auto& tc = topologyColors();

  painter->setBrush(tc.legendBackground);
  painter->setPen(QPen(tc.legendBorder, 0.5));
  painter->translate(x, y);
  painter->drawRoundedRect(0, 0, lw, lh, 4, 4);

  QFont f = painter->font();
  f.setPointSize(9);
  f.setBold(true);
  painter->setFont(f);
  painter->setPen(tc.legendText);
  painter->drawText(QRectF(8, 4, 90, 16), Qt::AlignLeft,
                    QStringLiteral("图例"));

  f.setBold(false);
  f.setPointSize(9);
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
    qreal ey = 24 + i * 16;
    painter->setBrush(entries[i].color);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QPointF(14, ey + 4), 4, 4);
    painter->setPen(tc.legendText);
    painter->drawText(QRectF(24, ey, 70, 14), Qt::AlignLeft, entries[i].label);
  }

  painter->restore();
}

void TopologyView::zoomIn() {
  qreal factor = 1.15;
  qreal newZoom = current_zoom_ * factor;
  if (newZoom >= 0.1 && newZoom <= 10.0) {
    current_zoom_ = newZoom;
    scale(factor, factor);
    emit zoomChanged(current_zoom_);
  }
}

void TopologyView::zoomOut() {
  qreal factor = 1.0 / 1.15;
  qreal newZoom = current_zoom_ * factor;
  if (newZoom >= 0.1 && newZoom <= 10.0) {
    current_zoom_ = newZoom;
    scale(factor, factor);
    emit zoomChanged(current_zoom_);
  }
}

void TopologyView::zoomReset() {
  current_zoom_ = 1.0;
  resetTransform();
  emit zoomChanged(current_zoom_);
}

void TopologyView::wheelEvent(QWheelEvent* event) {
  qreal factor = 1.0;
  if (event->angleDelta().y() > 0) {
    factor = 1.15;
  } else {
    factor = 1.0 / 1.15;
  }

  qreal newZoom = current_zoom_ * factor;
  if (newZoom >= 0.1 && newZoom <= 10.0) {
    current_zoom_ = newZoom;
    scale(factor, factor);
    emit zoomChanged(current_zoom_);
  }
  event->accept();
}

void TopologyView::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton) {
    panning_ = true;
    last_pan_point_ = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }
  QGraphicsView::mousePressEvent(event);
}

void TopologyView::mouseMoveEvent(QMouseEvent* event) {
  if (panning_) {
    QPoint delta = event->pos() - last_pan_point_;
    last_pan_point_ = event->pos();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    event->accept();
    return;
  }
  QGraphicsView::mouseMoveEvent(event);
}

void TopologyView::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton) {
    panning_ = false;
    setCursor(Qt::ArrowCursor);
    event->accept();
    return;
  }
  QGraphicsView::mouseReleaseEvent(event);
}

void TopologyView::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu;

  auto* s = qobject_cast<TopologyScene*>(this->scene());
  if (!s)
    return;

  QPointF scenePos = mapToScene(event->pos());
  auto* devPort = s->devicePortItemAt(scenePos);
  auto* uut = s->uutItemAt(scenePos);
  auto* dev = s->deviceItemAt(scenePos);
  auto* conn = s->connectionItemAt(scenePos);

  if (devPort) {
    auto* delAct = menu.addAction(QStringLiteral("删除端口"));
    connect(delAct, &QAction::triggered, this,
            [this, devPort]() { emit deleteItemRequested(devPort); });
  } else if (uut) {
    auto* act = menu.addAction(QStringLiteral("删除 UUT"));
    connect(act, &QAction::triggered, this,
            [this, uut]() { emit deleteItemRequested(uut); });
  } else if (dev) {
    auto* delAct = menu.addAction(QStringLiteral("删除设备"));
    connect(delAct, &QAction::triggered, this,
            [this, dev]() { emit deleteItemRequested(dev); });
    auto* saveAct = menu.addAction(QStringLiteral("另存为模板..."));
    connect(saveAct, &QAction::triggered, this,
            [this, dev]() { emit saveTemplateRequested(dev); });
  } else if (conn) {
    auto* delAct = menu.addAction(QStringLiteral("删除连线"));
    connect(delAct, &QAction::triggered, this,
            [this, conn]() { emit deleteItemRequested(conn); });
  } else {
    auto* addUutAct = menu.addAction(QStringLiteral("添加 UUT"));
    connect(addUutAct, &QAction::triggered, this,
            [this, scenePos]() { emit addUutRequested(scenePos); });
    auto* addDevAct = menu.addAction(QStringLiteral("添加设备"));
    connect(addDevAct, &QAction::triggered, this,
            [this, scenePos]() { emit addDeviceRequested(scenePos); });
    auto* addTmplAct = menu.addAction(QStringLiteral("从模板添加设备..."));
    connect(addTmplAct, &QAction::triggered, this,
            [this, scenePos]() { emit addDeviceFromTemplateRequested(scenePos); });
  }

  menu.exec(event->globalPos());
}

}  // namespace etest::topology
