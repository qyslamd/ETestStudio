#pragma once

#include <QGraphicsView>
#include <QHash>
#include <QRectF>
#include <QVector>

class QGraphicsScene;
class QGraphicsProxyWidget;

namespace etest::app {

class SignalVisualizer;

class VisualizationArea : public QGraphicsView {
  Q_OBJECT

 public:
  explicit VisualizationArea(QWidget* parent = nullptr);
  ~VisualizationArea() override;

  void addVisualizer(const QString& connectionId,
                     SignalVisualizer* visualizer);
  void removeVisualizer(const QString& connectionId);

  SignalVisualizer* visualizer(const QString& connectionId) const;

  // 编辑/展示两态：编辑态可拖拽/resize/排列，展示态只读并保持自动网格
  void setEditMode(bool edit);
  bool editMode() const { return edit_mode_; }

  // 布局收集/应用（scene 坐标，供编辑态读写卡片位置/大小）
  struct VisualizerGeometry {
    QString connectionId;
    QRectF rect;
  };
  QVector<VisualizerGeometry> visualizerGeometries() const;
  void setVisualizerGeometry(const QString& connectionId, const QRectF& rect);

  // 排列/分布（编辑态，操作选中卡片，移植拓扑 doAlign/doDistribute）
  enum class AlignType { Left, HCenter, Right, Top, VCenter, Bottom };
  enum class DistributeType { Horizontal, Vertical };
  void alignVisualizers(AlignType type);
  void distributeVisualizers(DistributeType type);
  int selectedVisualizerCount() const;

  void clearAll();
  int visualizerCount() const { return items_.size(); }

  QList<QString> activeChannels() const;

 signals:
  void visualizerClosed(const QString& connectionId);
  // 布局被改动（用户拖拽/resize、排列/分布），供宿主置脏
  void layoutChanged();

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  // 视图浏览（参考 TopologyView）：Ctrl+滚轮缩放 + 中键平移
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  void relayout();

  struct Item {
    QGraphicsProxyWidget* proxy = nullptr;
    SignalVisualizer* widget = nullptr;
  };

  QGraphicsScene* scene_ = nullptr;
  QHash<QString, Item> items_;
  bool edit_mode_ = false;  // 默认展示态（保持现有自动网格行为）
  bool panning_ = false;    // 中键平移中
  QPoint last_pan_point_;
};

}  // namespace etest::app
