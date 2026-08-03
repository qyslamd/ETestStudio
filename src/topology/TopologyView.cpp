#include "TopologyView.h"
#include "TopologyScene.h"
#include "TopologyTheme.h"
#include "topology_items.h"

#include "ThemeManager.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include "logger/Logger.h"

namespace etest::topology {

TopologyView::TopologyView(TopologyScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent) {
  setRenderHint(QPainter::Antialiasing);
  setDragMode(QGraphicsView::RubberBandDrag);
  setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
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

  renderLegendCache();

  connect(&etest::core_ui::ThemeManager::instance(),
          &etest::core_ui::ThemeManager::themeChanged, this, [this](bool) {
            setBackgroundBrush(topologyColors().sceneBackground);
            renderLegendCache();
            viewport()->update();
          });
}

void TopologyView::paintEvent(QPaintEvent* event) {
  QGraphicsView::paintEvent(event);

  // Blit cached legend — pixel copy only, no vector ops
  if (!legend_cache_.isNull()) {
    QPainter painter(viewport());
    int x = viewport()->width() - legend_cache_.width() - 10;
    int y = viewport()->height() - legend_cache_.height() - 10;
    painter.drawPixmap(x, y, legend_cache_);
  }
}

void TopologyView::resizeEvent(QResizeEvent* event) {
  QGraphicsView::resizeEvent(event);
  renderLegendCache();   // rebuild cache at new size
}

void TopologyView::renderLegendCache() {
  const int lw = 110, lh = 148;
  const auto& tc = topologyColors();

  legend_cache_ = QPixmap(lw, lh);
  legend_cache_.fill(Qt::transparent);

  QPainter p(&legend_cache_);
  p.setRenderHint(QPainter::Antialiasing);

  // Background
  p.setBrush(tc.legendBackground);
  p.setPen(QPen(tc.legendBorder, 0.5));
  p.drawRoundedRect(0, 0, lw - 1, lh - 1, 4, 4);

  // Title
  QFont f = p.font();
  f.setPointSize(9);
  f.setBold(true);
  p.setFont(f);
  p.setPen(tc.legendText);
  p.drawText(QRectF(8, 4, 100, 16), Qt::AlignLeft, QStringLiteral("图例"));

  f.setBold(false);
  f.setPointSize(7);
  p.setFont(f);

  // Port direction entries
  struct PortEntry { QColor color; QString label; };
  PortEntry ports[] = {
      {tc.directionInput, QStringLiteral("输入")},
      {tc.directionOutput, QStringLiteral("输出")},
      {tc.directionBidirectional, QStringLiteral("双向")},
  };
  for (int i = 0; i < 3; ++i) {
    qreal ey = 24 + i * 16;
    p.setBrush(ports[i].color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(14, ey + 4), 4, 4);
    p.setPen(tc.legendText);
    p.drawText(QRectF(24, ey, 80, 14), Qt::AlignLeft, ports[i].label);
  }

  // Divider
  qreal yDiv = 76;
  p.setPen(QPen(tc.legendBorder, 1.0));
  p.drawLine(QPointF(8, yDiv), QPointF(lw - 8, yDiv));

  // Device type entries
  struct DevEntry {
    QColor fill;
    QColor border;
    QString label;
  };
  DevEntry devs[] = {
      {tc.deviceFill, tc.deviceBorder, QStringLiteral("激励设备")},
      {tc.uutFill, tc.uutBorder, QStringLiteral("UUT")},
  };
  for (int i = 0; i < 2; ++i) {
    qreal ey = 84 + i * 18;
    p.setBrush(devs[i].fill);
    p.setPen(QPen(devs[i].border, 1.5));
    p.drawRoundedRect(QRectF(8, ey + 1, 14, 10), 2, 2);
    p.setPen(tc.legendText);
    p.drawText(QRectF(28, ey, 76, 14), Qt::AlignLeft, devs[i].label);
  }

  p.end();
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

void TopologyView::zoomFit() {
  auto rect = scene()->itemsBoundingRect();
  if (rect.isEmpty()) return;
  rect.adjust(-40, -40, 40, 40);
  fitInView(rect, Qt::KeepAspectRatio);
  current_zoom_ = transform().m11();
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
            [this, devPort]() { LOG_INFO("TOPOLOGY_UI", "右键删除设备端口"); emit deleteItemRequested(devPort); });
  } else if (uutPort) {
    auto* delUutPortAct = menu.addAction(QStringLiteral("删除端口"));
    connect(delUutPortAct, &QAction::triggered, this,
            [this, uutPort]() { LOG_INFO("TOPOLOGY_UI", "右键删除 UUT 端口"); emit deleteItemRequested(uutPort); });
  } else if (uut) {
    auto* addPortAct = menu.addAction(QStringLiteral("添加端口"));
    connect(addPortAct, &QAction::triggered, this,
            [this, uut]() { LOG_INFO("TOPOLOGY_UI", "右键添加端口"); emit addUutPortRequested(uut->productIndex()); });
    menu.addSeparator();
    auto* act = menu.addAction(QStringLiteral("删除 UUT"));
    connect(act, &QAction::triggered, this,
            [this, uut]() { LOG_INFO("TOPOLOGY_UI", "右键删除 UUT"); emit deleteItemRequested(uut); });
  } else if (dev) {
    auto* delAct = menu.addAction(QStringLiteral("删除设备"));
    connect(delAct, &QAction::triggered, this,
            [this, dev]() { LOG_INFO("TOPOLOGY_UI", "右键删除设备"); emit deleteItemRequested(dev); });
    auto* saveAct = menu.addAction(QStringLiteral("另存为模板..."));
    connect(saveAct, &QAction::triggered, this,
            [this, dev]() { LOG_INFO("TOPOLOGY_UI", "右键保存模板"); emit saveTemplateRequested(dev); });
  } else if (conn) {
    auto* delAct = menu.addAction(QStringLiteral("删除连线"));
    connect(delAct, &QAction::triggered, this,
            [this, conn]() { LOG_INFO("TOPOLOGY_UI", "右键删除连线"); emit deleteItemRequested(conn); });
  } else {
    auto* addUutAct = menu.addAction(QStringLiteral("添加 UUT"));
    connect(addUutAct, &QAction::triggered, this,
            [this, scenePos]() { LOG_INFO("TOPOLOGY_UI", "右键添加 UUT"); emit addUutRequested(scenePos); });
    auto* addDevAct = menu.addAction(QStringLiteral("添加设备"));
    connect(addDevAct, &QAction::triggered, this,
            [this, scenePos]() { LOG_INFO("TOPOLOGY_UI", "右键添加设备"); emit addDeviceRequested(scenePos); });
    auto* addTmplAct = menu.addAction(QStringLiteral("从模板添加设备..."));
    connect(addTmplAct, &QAction::triggered, this,
            [this, scenePos]() { LOG_INFO("TOPOLOGY_UI", "右键从模板添加设备"); emit addDeviceFromTemplateRequested(scenePos); });
  }

  menu.exec(event->globalPos());
}

}  // namespace etest::topology
