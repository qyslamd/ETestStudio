#ifndef ETEST_APP_EXECUTION_DASHBOARD_H_
#define ETEST_APP_EXECUTION_DASHBOARD_H_

#include <QWidget>

class QSplitter;
class QTabWidget;

namespace etest::app {

class ExecutionDebugWidget;
class SignalTreePanel;
class VisualizationArea;
class ExecutionOutputPanel;
class ProblemsPanel;

// ══════════════════════════════════════════════════════════════════════════════
// ExecutionDashboard - page 1 执行仪表盘
// ══════════════════════════════════════════════════════════════════════════════
// 三列水平布局（执行调试 | 信号树 | 可视化区）+ 底部输出/问题 tab 面板。
// 在 run 模式切换时由 MainWindow 构建并设置到 central_stack_ page 1。
// ══════════════════════════════════════════════════════════════════════════════
class ExecutionDashboard : public QWidget {
  Q_OBJECT

 public:
  explicit ExecutionDashboard(QWidget* parent = nullptr);

  ExecutionDebugWidget* debugWidget() const { return debug_widget_; }
  SignalTreePanel* signalTreePanel() const { return signal_tree_; }
  VisualizationArea* visualizationArea() const { return vis_area_; }
  ExecutionOutputPanel* outputPanel() const { return output_panel_; }
  /// 底部「问题」tab 的 ProblemsPanel（内部创建并持有）
  ProblemsPanel* problemsPanel() const { return problems_panel_; }

  // ── 设置底部输出面板（由 MainWindow 注入，替代自建实例） ──
  void setOutputPanel(ExecutionOutputPanel* panel);

  /// 程序化切换底部 tab（0=输出, 1=问题）
  /// 注：索引语义依赖 setOutputPanel 已执行（否则只有「问题」tab 在 index 0）
  void setCurrentBottomTab(int index);

  /// 切到「问题」tab（语义化，不依赖 tab 索引硬编码）
  void showProblemsTab();

 private:
  void initUi();

  ExecutionDebugWidget* debug_widget_ = nullptr;
  SignalTreePanel* signal_tree_ = nullptr;
  VisualizationArea* vis_area_ = nullptr;
  ExecutionOutputPanel* output_panel_ = nullptr;
  ProblemsPanel* problems_panel_ = nullptr;
  QSplitter* main_splitter_ = nullptr;
  QSplitter* vert_splitter_ = nullptr;  // 主区域 + 底部输出/问题 tab 垂直 splitter
  QTabWidget* bottom_tabs_ = nullptr;   // 底部输出/问题 tab
};

}  // namespace etest::app

#endif  // ETEST_APP_EXECUTION_DASHBOARD_H_
