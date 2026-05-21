#include "TopologyView.h"
#include "TopologyScene.h"
#include "TopologyTheme.h"
#include "topology_items.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

namespace etest::topology {

TopologyView::TopologyView(TopologyScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent) {
  setRenderHint(QPainter::Antialiasing);
  setDragMode(QGraphicsView::RubberBandDrag);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  // Allow zooming into empty areas
  setInteractive(true);

  // Style — adapt to current theme
  setBackgroundBrush(topologyColors().sceneBackground);
  setFrameShape(QFrame::NoFrame);

  // Accept drops for device palette drag-and-drop
  setAcceptDrops(true);
}

void TopologyView::paintEvent(QPaintEvent* event) {
  QGraphicsView::paintEvent(event);

  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing);

  int lw = 100, lh = 75;
  int x = viewport()->width() - lw - 10;
  int y = viewport()->height() - lh - 10;

  const auto& tc = topologyColors();

  painter.setBrush(tc.legendBackground);
  painter.setPen(QPen(tc.legendBorder, 0.5));
  painter.translate(x, y);
  painter.drawRoundedRect(0, 0, lw, lh, 4, 4);

  QFont f = painter.font();
  f.setPointSize(9);
  f.setBold(true);
  painter.setFont(f);
  painter.setPen(tc.legendText);
  painter.drawText(QRectF(8, 4, 90, 16), Qt::AlignLeft,
                   QStringLiteral("图例"));

  f.setBold(false);
  f.setPointSize(9);
  painter.setFont(f);

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
    painter.setBrush(entries[i].color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(14, ey + 4), 4, 4);
    painter.setPen(tc.legendText);
    painter.drawText(QRectF(24, ey, 70, 14), Qt::AlignLeft, entries[i].label);
  }
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
  if (!(event->modifiers() & Qt::ControlModifier)) {
    QGraphicsView::wheelEvent(event);
    return;
  }

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
  auto* uutPort = s->portItemAt(scenePos);
  auto* uut = s->uutItemAt(scenePos);
  auto* dev = s->deviceItemAt(scenePos);
  auto* conn = s->connectionItemAt(scenePos);

  if (devPort) {
    auto* delAct = menu.addAction(QStringLiteral("删除端口"));
    connect(delAct, &QAction::triggered, this,
            [this, devPort]() { emit deleteItemRequested(devPort); });

    menu.addSeparator();
    auto* portStyleMenu = menu.addMenu(QStringLiteral("端口样式"));
    auto* portGroup = new QActionGroup(portStyleMenu);
    portGroup->setExclusive(true);
    auto* circAct = portStyleMenu->addAction(QStringLiteral("圆形"));
    circAct->setCheckable(true);
    circAct->setChecked(devPort->portStyle() == PortStyle::Circle);
    portGroup->addAction(circAct);
    auto* triAct = portStyleMenu->addAction(QStringLiteral("三角形"));
    triAct->setCheckable(true);
    triAct->setChecked(devPort->portStyle() == PortStyle::Triangle);
    portGroup->addAction(triAct);
    connect(circAct, &QAction::triggered, this,
            [devPort]() { devPort->setPortStyle(PortStyle::Circle); });
    connect(triAct, &QAction::triggered, this,
            [devPort]() { devPort->setPortStyle(PortStyle::Triangle); });
  } else if (uutPort) {
    auto* portStyleMenu = menu.addMenu(QStringLiteral("端口样式"));
    auto* portGroup = new QActionGroup(portStyleMenu);
    portGroup->setExclusive(true);
    auto* circAct = portStyleMenu->addAction(QStringLiteral("圆形"));
    circAct->setCheckable(true);
    circAct->setChecked(uutPort->portStyle() == PortStyle::Circle);
    portGroup->addAction(circAct);
    auto* triAct = portStyleMenu->addAction(QStringLiteral("三角形"));
    triAct->setCheckable(true);
    triAct->setChecked(uutPort->portStyle() == PortStyle::Triangle);
    portGroup->addAction(triAct);
    connect(circAct, &QAction::triggered, this,
            [uutPort]() { uutPort->setPortStyle(PortStyle::Circle); });
    connect(triAct, &QAction::triggered, this,
            [uutPort]() { uutPort->setPortStyle(PortStyle::Triangle); });
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

    menu.addSeparator();

    auto* styleMenu = menu.addMenu(QStringLiteral("连线样式"));
    auto* group = new QActionGroup(styleMenu);
    group->setExclusive(true);

    auto* bezierAct = styleMenu->addAction(QStringLiteral("曲线"));
    bezierAct->setCheckable(true);
    bezierAct->setChecked(conn->style() == PathStyle::Bezier);
    group->addAction(bezierAct);

    auto* polyAct = styleMenu->addAction(QStringLiteral("折线"));
    polyAct->setCheckable(true);
    polyAct->setChecked(conn->style() == PathStyle::Polyline);
    group->addAction(polyAct);

    auto* lineAct = styleMenu->addAction(QStringLiteral("直线"));
    lineAct->setCheckable(true);
    lineAct->setChecked(conn->style() == PathStyle::Straight);
    group->addAction(lineAct);

    connect(bezierAct, &QAction::triggered, this,
            [conn]() { conn->setStyle(PathStyle::Bezier); });
    connect(polyAct, &QAction::triggered, this,
            [conn]() { conn->setStyle(PathStyle::Polyline); });
    connect(lineAct, &QAction::triggered, this,
            [conn]() { conn->setStyle(PathStyle::Straight); });
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
