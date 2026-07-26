#ifndef ETEST_APP_SIGNAL_TREE_PANEL_H_
#define ETEST_APP_SIGNAL_TREE_PANEL_H_

#include <QHash>
#include <QWidget>

#include "engine/MonitorManager.h"

class QListWidget;
class QListWidgetItem;
class QLineEdit;

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// SignalTreePanel — 监听器通道导航面板
// ══════════════════════════════════════════════════════════════════════════════
// 扁平化 QListWidget，每行一个监听器 + checkbox，用于勾选要观察的通道。
// 顶部搜索框支持按名称模糊过滤。每行右侧显示最新值的缩略文本。
// ══════════════════════════════════════════════════════════════════════════════
class SignalTreePanel : public QWidget {
  Q_OBJECT

 public:
  explicit SignalTreePanel(QWidget* parent = nullptr);

  void setMonitorTree(
      const QList<etest::engine::MonitorManager::MonitorTreeEntry>& tree,
      const QList<int>& preCheckedMonitors = {});

  void updateNodeValue(int monitorIndex, const QString& valueText);
  void uncheckMonitor(int monitorIndex);
  void clearTree();

 signals:
  void checkStateChanged(int monitorIndex, bool checked);

 private:
  void initUi();
  void buildTree(
      const QList<etest::engine::MonitorManager::MonitorTreeEntry>& tree,
      const QList<int>& preCheckedMonitors);
  void onFilterChanged(const QString& text);

  QLineEdit* search_box_ = nullptr;
  QListWidget* list_ = nullptr;
  // monitorIndex -> QListWidgetItem
  QHash<int, QListWidgetItem*> node_map_;
  QList<etest::engine::MonitorManager::MonitorTreeEntry> tree_data_;
};

}  // namespace etest::app

#endif  // ETEST_APP_SIGNAL_TREE_PANEL_H_
