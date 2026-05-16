#include "TopologyView.h"
#include "TopologyScene.h"
#include "topology_items.h"

#include <QWheelEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QScrollBar>

namespace topology {

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

    // Style
    setBackgroundBrush(QColor(250, 250, 250));
    setFrameShape(QFrame::NoFrame);
}

void TopologyView::zoomIn() {
    qreal factor = 1.15;
    qreal newZoom = current_zoom_ * factor;
    if (newZoom >= 0.1 && newZoom <= 10.0) {
        current_zoom_ = newZoom;
        scale(factor, factor);
    }
}

void TopologyView::zoomOut() {
    qreal factor = 1.0 / 1.15;
    qreal newZoom = current_zoom_ * factor;
    if (newZoom >= 0.1 && newZoom <= 10.0) {
        current_zoom_ = newZoom;
        scale(factor, factor);
    }
}

void TopologyView::zoomReset() {
    current_zoom_ = 1.0;
    resetTransform();
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
    if (!s) return;

    QPointF scenePos = mapToScene(event->pos());
    auto* devPort = s->devicePortItemAt(scenePos);
    auto* uut = s->uutItemAt(scenePos);
    auto* dev = s->deviceItemAt(scenePos);
    auto* conn = s->connectionItemAt(scenePos);

    if (devPort) {
        auto* delAct = menu.addAction(QStringLiteral("删除端口"));
        connect(delAct, &QAction::triggered, this, [this, devPort]() {
            emit deleteItemRequested(devPort);
        });
    } else if (uut) {
        auto* act = menu.addAction(QStringLiteral("删除 UUT"));
        connect(act, &QAction::triggered, this, [this, uut]() {
            emit deleteItemRequested(uut);
        });
    } else if (dev) {
        auto* delAct = menu.addAction(QStringLiteral("删除设备"));
        connect(delAct, &QAction::triggered, this, [this, dev]() {
            emit deleteItemRequested(dev);
        });
        auto* saveAct = menu.addAction(QStringLiteral("另存为模板..."));
        connect(saveAct, &QAction::triggered, this, [this, dev]() {
            emit saveTemplateRequested(dev);
        });
    } else if (conn) {
        auto* delAct = menu.addAction(QStringLiteral("删除连线"));
        connect(delAct, &QAction::triggered, this, [this, conn]() {
            emit deleteItemRequested(conn);
        });
    } else {
        auto* addUutAct = menu.addAction(QStringLiteral("添加 UUT"));
        connect(addUutAct, &QAction::triggered, this, [this, scenePos]() {
            emit addUutRequested(scenePos);
        });
        auto* addDevAct = menu.addAction(QStringLiteral("添加设备"));
        connect(addDevAct, &QAction::triggered, this, [this, scenePos]() {
            emit addDeviceRequested(scenePos);
        });
    }

    menu.exec(event->globalPos());
}

}  // namespace topology
