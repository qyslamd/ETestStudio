#ifndef ETEST_APP_EXECUTION_DASHBOARD_H_
#define ETEST_APP_EXECUTION_DASHBOARD_H_

#include <QWidget>

class QSplitter;

namespace etest::app {

class RunStatusPanel;
class SignalTreePanel;
class VisualizationArea;
class ExecutionOutputPanel;

// ══════════════════════════════════════════════════════════════════════════════
// ExecutionDashboard — page 1 执行仪表盘
// ══════════════════════════════════════════════════════════════════════════════
// 三列水平布局（运行状态 | 信号树 | 可视化区）+ 底部运行输出面板。
// 在 run 模式切换时由 MainWindow 构建并设置到 central_stack_ page 1。
// ══════════════════════════════════════════════════════════════════════════════
class ExecutionDashboard : public QWidget {
  Q_OBJECT

 public:
  explicit ExecutionDashboard(QWidget* parent = nullptr);

  RunStatusPanel* runStatusPanel() const { return run_status_; }
  SignalTreePanel* signalTreePanel() const { return signal_tree_; }
  VisualizationArea* visualizationArea() const { return vis_area_; }
  ExecutionOutputPanel* outputPanel() const { return output_panel_; }

  // ── 设置底部输出面板（由 MainWindow 注入，替代自建实例） ──
  void setOutputPanel(ExecutionOutputPanel* panel);

 private:
  void initUi();

  RunStatusPanel* run_status_ = nullptr;
  SignalTreePanel* signal_tree_ = nullptr;
  VisualizationArea* vis_area_ = nullptr;
  ExecutionOutputPanel* output_panel_ = nullptr;
  QSplitter* main_splitter_ = nullptr;
  QSplitter* vert_splitter_ = nullptr;  // 主区域 + 底部输出面板垂直 splitter
};

}  // namespace etest::app

#endif  // ETEST_APP_EXECUTION_DASHBOARD_H_
