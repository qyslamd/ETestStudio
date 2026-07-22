#ifndef ETEST_APP_VISUALIZATION_AREA_H_
#define ETEST_APP_VISUALIZATION_AREA_H_

#include <QGraphicsView>
#include <QHash>
#include <QPair>

class QGraphicsScene;
class QGraphicsProxyWidget;

namespace etest::app {

class SignalVisualizer;

// ══════════════════════════════════════════════════════════════════════════════
// VisualizationArea — 信号可视化组件的自动网格容器
// ══════════════════════════════════════════════════════════════════════════════
// 基于 QGraphicsView + QGraphicsScene，以 QGraphicsProxyWidget 包裹每个
// SignalVisualizer，按自动网格排列。通过 addVisualizer / removeVisualizer 管理
// 生命周期，每次变更后自动调用 relayout() 重新计算网格位置。
// ══════════════════════════════════════════════════════════════════════════════
class VisualizationArea : public QGraphicsView {
  Q_OBJECT

 public:
  explicit VisualizationArea(QWidget* parent = nullptr);
  ~VisualizationArea() override;

  // ── 添加/移除可视化组件 ──
  void addVisualizer(int monitorIndex, int channelIndex,
                     SignalVisualizer* visualizer);
  void removeVisualizer(int monitorIndex, int channelIndex);

  // ── 按索引查找 ──
  SignalVisualizer* visualizer(int monitorIndex, int channelIndex) const;

  // ── 清空所有 ──
  void clearAll();

  // ── 当前数量 ──
  int visualizerCount() const { return items_.size(); }

  // ── 当前活跃通道列表（供 syncProjectTopologies 恢复订阅使用） ──
  QList<QPair<int, int>> activeChannels() const;

 signals:
  // ── 从可视化区内关闭某个组件（右键菜单等内部操作） ──
  void visualizerClosed(int monitorIndex, int channelIndex);

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  // ── 重新计算网格布局 ──
  void relayout();

  // ── 内部条目 ──
  struct Item {
    QGraphicsProxyWidget* proxy = nullptr;
    SignalVisualizer* widget = nullptr;
  };

  QGraphicsScene* scene_ = nullptr;
  // key = (monitorIndex << 16) | channelIndex
  QHash<int, Item> items_;
};

}  // namespace etest::app

#endif  // ETEST_APP_VISUALIZATION_AREA_H_
