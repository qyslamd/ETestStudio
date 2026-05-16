#pragma once

#include <QGraphicsView>

namespace topology {

class TopologyScene;

class TopologyView : public QGraphicsView {
    Q_OBJECT
public:
    explicit TopologyView(TopologyScene* scene, QWidget* parent = nullptr);

    void zoomIn();
    void zoomOut();
    void zoomReset();

signals:
    void addUutRequested(QPointF scenePos);
    void addDeviceRequested(QPointF scenePos);
    void deleteItemRequested(QGraphicsItem* item);
    void saveTemplateRequested(QGraphicsItem* item);

protected:
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    qreal current_zoom_ = 1.0;
    bool panning_ = false;
    QPoint last_pan_point_;
};

}  // namespace topology
