#ifndef ETEST_APP_SIGNAL_TREE_PANEL_H_
#define ETEST_APP_SIGNAL_TREE_PANEL_H_

#include <QHash>
#include <QPair>
#include <QWidget>

#include "engine/MonitorManager.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// SignalTreePanel — 监听器通道导航树
// ══════════════════════════════════════════════════════════════════════════════
// 按监听器→通道分组的树状导航，每个叶节点有 checkbox 用于固定到可视化区。
// 顶部搜索框支持按名称/设备类型模糊过滤。
// 每个叶节点右侧显示最新值的缩略文本。
// ══════════════════════════════════════════════════════════════════════════════
class SignalTreePanel : public QWidget {
  Q_OBJECT

 public:
  explicit SignalTreePanel(QWidget* parent = nullptr);

  // ── 设置树数据（来自 MonitorManager::monitorTree()） ──
  void setMonitorTree(
      const QList<etest::engine::MonitorManager::MonitorTreeEntry>& tree,
      const QList<QPair<int, int>>& preCheckedChannels = {});

  // ── 更新某个通道的实时值缩略文本 ──
  void updateNodeValue(int monitorIndex, int channelIndex,
                        const QString& valueText);

  // ── 取消勾选某个通道（用于可视化区右键关闭时同步） ──
  void uncheckChannel(int monitorIndex, int channelIndex);

  // ── 清空树和所有勾选（项目关闭时使用） ──
  void clearTree();

 signals:
  // ── 叶节点 checkbox 状态变化 ──
  void checkStateChanged(int monitorIndex, int channelIndex, bool checked);

 private:
  void initUi();
  void buildTree(
      const QList<etest::engine::MonitorManager::MonitorTreeEntry>& tree,
      const QList<QPair<int, int>>& preCheckedChannels = {});
  void onFilterChanged(const QString& text);

  QLineEdit* search_box_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  // key = (monitorIndex << 16) | channelIndex
  QHash<int, QTreeWidgetItem*> node_map_;
  // 原始树数据缓存，预留备用（搜索过滤通过遍历 tree_ 实现）
  QList<etest::engine::MonitorManager::MonitorTreeEntry> tree_data_;
};

}  // namespace etest::app

#endif  // ETEST_APP_SIGNAL_TREE_PANEL_H_
