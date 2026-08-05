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

  // 编辑模式（便捷入口）：布局手动 + 交互可编辑（组合下面两个开关）
  void setEditMode(bool edit);
  // 布局模式：true 手动摆放（不自动重排，供 .erun.layout 应用）；false 自动网格
  void setManualLayout(bool manual);
  // 交互开关：true 可拖拽/resize/选中；false 只读（禁拖拽/resize/手柄）
  void setInteractive(bool interactive);
  bool editMode() const { return interactive_; }

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
  bool manual_layout_ = false;  // 默认自动网格（展示态）
  bool interactive_ = false;    // 默认只读（展示态）
  bool panning_ = false;        // 中键平移中
  QPoint last_pan_point_;
};

}  // namespace etest::app
