#pragma once

#include <QDialog>

#include "engine/MonitorManager.h"

namespace etest::app {

class SignalTreePanel;

// ══════════════════════════════════════════════════════════════════════════════
// ChannelSelectionDialog — 监听器通道选择 Modal Dialog
// ══════════════════════════════════════════════════════════════════════════════
// 只负责展示通道树（内部持有 SignalTreePanel）并转发勾选状态，单调减对
// MonitorManager / 可视化组件的编排逻辑，保持其在调用方（ExecutionPanelController）。
class ChannelSelectionDialog : public QDialog {
  Q_OBJECT

 public:
  explicit ChannelSelectionDialog(QWidget* parent = nullptr);

  void setMonitorTree(
      const QList<etest::engine::MonitorManager::MonitorTreeEntry>& tree,
      const QList<int>& preCheckedMonitors = {});
  void uncheckMonitor(int monitorIndex);
  void clearTree();

 signals:
  void checkStateChanged(int monitorIndex, bool checked);

 private:
  void initUi();

  SignalTreePanel* tree_panel_ = nullptr;
};

}  // namespace etest::app