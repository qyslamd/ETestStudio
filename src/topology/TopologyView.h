#pragma once

#include <QGraphicsView>
#include <QPixmap>

#include "TopologyPathRouter.h"
#include "topology_items.h"

namespace etest::topology {

class TopologyScene;

class TopologyView : public QGraphicsView {
  Q_OBJECT
 public:
  explicit TopologyView(TopologyScene* scene, QWidget* parent = nullptr);

  void zoomIn();
  void zoomOut();
  void zoomReset();
  void zoomFit();

 signals:
  void addUutRequested(QPointF scenePos);
  void addDeviceRequested(QPointF scenePos);
  void addUutPortRequested(int productIndex);
  void deleteItemRequested(QGraphicsItem* item);
  void addMonitorRequested(ConnectionItem* item);
  void saveTemplateRequested(QGraphicsItem* item);
  void addDeviceFromTemplateRequested(QPointF scenePos);
  void productPortStyleChangeRequested(int productIndex, int portIndex,
                                       PortStyle style);
  void devicePortStyleChangeRequested(int deviceIndex, int portIndex,
                                      PortStyle style);
  void connectionStyleChangeRequested(int connectionIndex, PathStyle style);
  void zoomChanged(qreal zoomFactor);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private:
  void renderLegendCache();
  qreal current_zoom_ = 1.0;
  bool panning_ = false;
  QPoint last_pan_point_;
  QPixmap legend_cache_;
};

}  // namespace etest::topology
