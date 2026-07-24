#ifndef ETEST_APP_EXECUTIONPANELCONTROLLER_H_
#define ETEST_APP_EXECUTIONPANELCONTROLLER_H_

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include "widgets/ProblemsPanel.h"  // NavTarget 定义（navigateRequested 信号参数）

class QAction;
class QDialog;
class QLabel;
class QStackedWidget;
class QWidget;

namespace etest::app {

class AppStatusBarController;
class EditorManager;
class ExecutionDebugWidget;
class ExecutionOutputPanel;
class TestProgramManagerWidget;
class ExecutionDashboard;
class ProgramSelectionPopup;
class SignalTreePanel;

}  // namespace etest::app

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

#include "engine/TestExecutionEngine.h"

namespace etest::engine {
class MonitorSample;
}  // namespace etest::engine

namespace icd {
class Repository;
}  // namespace icd

namespace etest::app {

class ExecutionPanelController : public QObject {
  Q_OBJECT
 public:
  explicit ExecutionPanelController(QWidget* parent_widget,
                                    QObject* parent = nullptr);

  // 两步初始化：Constructor 只创建 QAction，postInit 补全依赖
  // 注：test_program_mgr 参数已废弃（阶段二迁移至 popup），保留参数位仅作过渡，
  // 后续可一并清理签名。problems_panel/bottom_container 已于阶段三移除。
  void postInit(ExecutionOutputPanel* output_panel,
                etest::core::SignalRegistry* signal_registry,
                std::shared_ptr<icd::Repository> icd_repository,
                EditorManager* editor_mgr,
                TestProgramManagerWidget* test_program_mgr,
                AppStatusBarController* status_bar_ctrl);

  // 引擎生存期
  void createEngine();
  void destroyEngine();
  etest::engine::TestExecutionEngine* engine() const { return engine_; }

  // 运行时更新 ICD 上下文（项目打开时调用，createEngine 依赖它们）
  void updateIcdContext(etest::core::SignalRegistry* signal_registry,
                        std::shared_ptr<icd::Repository> icd_repository);

  // 执行控制
  void run();
  void pause();
  void stop();
  void verify();
  void runAll();
  void runNextInQueue();
  /// 仅更新 run/runAll 的 enable 状态（不重算 verify）
  void updateRunControls();

  // 状态
  void syncControlStates();

  // 中央堆叠容器注入（运行时切页）
  void setCentralStack(QStackedWidget* stack);

  // 执行仪表盘注入（用于连接监听器信号）
  void setDashboard(ExecutionDashboard* dashboard);
  ExecutionDashboard* dashboard() const { return dashboard_; }

  // 拓扑 / 监听器生命周期（由 MainWindow 在项目打开/关闭时调用）
  void syncProjectTopologies();
  void clearProjectState();
  /// 清空 MonitorManager 结构与运行时数据（拓扑变化时调用）
  void clearMonitorState();

  // 清空可视化组件采样数据（Ribbon 按钮触发）
  void clearData();

  // MonitorManager 访问器（由 controller 持有，跨引擎重建保持）
  etest::engine::MonitorManager* monitorManager() const { return monitor_manager_; }

  // Ribbon 动作（供 MainWindow setupRibbon 获取）
  QAction* runAction() const { return act_run_; }
  QAction* pauseAction() const { return act_pause_; }
  QAction* stopAction() const { return act_stop_; }
  QAction* verifyAction() const { return act_verify_; }
  QAction* runAllAction() const { return act_run_all_; }
  QLabel* ribbonStatsLabel() const { return label_ribbon_stats_; }

  // 程序选择 popup（供 MainWindow 放入 ribbon）
  ProgramSelectionPopup* programPopup() const { return popup_; }

  // 通道选择 action（供 MainWindow 放入 ribbon）
  QAction* selectChannelsAction() const { return act_select_channels_; }

  /// 弹出通道选择 Modal Dialog
  void showChannelSelectionDialog();

  // 清空数据按钮（供 MainWindow 放入 ribbon）
  QAction* clearDataAction() const { return act_clear_data_; }

 signals:
  void engineStateChanged(etest::engine::EngineState state);
  void execStatsUpdated(int pass, int fail, int elapsed);
  void preconditionResult(bool can_run);
  /// 用户双击问题项请求导航，MainWindow 接收执行 page0 跳转
  void navigateRequested(NavTarget target);

 private:
  void connectEngineSignals();
  void refreshMonitorTree();
  bool checkCanVerify() const;
  bool checkCanRun() const;
  bool checkCanRunAll() const;
  /// 运行前检测未保存文件并提示，返回 true 表示可以继续
  bool checkUnsavedAndPrompt(const QStringList& paths) const;
  // 引擎
  etest::engine::TestExecutionEngine* engine_ = nullptr;

  // Ribbon QAction（构造时创建，父对象为 MainWindow）
  QAction* act_run_ = nullptr;
  QAction* act_pause_ = nullptr;
  QAction* act_stop_ = nullptr;
  QAction* act_verify_ = nullptr;
  QAction* act_run_all_ = nullptr;
  QAction* act_clear_data_ = nullptr;
  QAction* act_select_channels_ = nullptr;
  QLabel* label_ribbon_stats_ = nullptr;

  // 引擎状态
  int pass_count_ = 0;
  int fail_count_ = 0;
  QString current_program_name_;
  QStringList run_queue_;

  // topology 文件 mtime 快照（供 syncProjectTopologies 判断变化）
  QHash<QString, QDateTime> topo_mtimes_;

  // 外部依赖（通过 postInit 注入）
  QWidget* parent_widget_ = nullptr;
  ExecutionDebugWidget* debug_widget_ = nullptr;
  ExecutionOutputPanel* output_panel_ = nullptr;
  etest::core::SignalRegistry* signal_registry_ = nullptr;
  std::shared_ptr<icd::Repository> icd_repository_;
  EditorManager* editor_mgr_ = nullptr;
  AppStatusBarController* status_bar_ctrl_ = nullptr;
  QStackedWidget* central_stack_ = nullptr;
  ExecutionDashboard* dashboard_ = nullptr;
  ProgramSelectionPopup* popup_ = nullptr;

  // SignalTreePanel（供通道选择 Dialog 使用，parent = nullptr，由 Dialog 自动 reparent）
  SignalTreePanel* signal_tree_ = nullptr;
  QDialog* signal_tree_dialog_ = nullptr;

  // MonitorManager（由 controller 持有，跨引擎重建保持；注入到引擎使用）
  etest::engine::MonitorManager* monitor_manager_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_EXECUTIONPANELCONTROLLER_H_
