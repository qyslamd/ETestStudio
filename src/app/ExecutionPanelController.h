#ifndef ETEST_APP_EXECUTIONPANELCONTROLLER_H_
#define ETEST_APP_EXECUTIONPANELCONTROLLER_H_

#include <QObject>
#include <QString>
#include <QStringList>

class QAction;
class QLabel;
class QStackedWidget;
class QWidget;

namespace etest::app {

class AppStatusBarController;
class EditorManager;
class ExecutionDebugWidget;
class ExecutionOutputPanel;
class TestProgramManagerWidget;
class ProblemsPanel;
class BottomContainerWidget;
class ExecutionDashboard;

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
  void postInit(ExecutionOutputPanel* output_panel,
                etest::core::SignalRegistry* signal_registry,
                std::shared_ptr<icd::Repository> icd_repository,
                EditorManager* editor_mgr,
                TestProgramManagerWidget* test_program_mgr,
                ProblemsPanel* problems_panel,
                BottomContainerWidget* bottom_container,
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

  // Ribbon 动作（供 MainWindow setupRibbon 获取）
  QAction* runAction() const { return act_run_; }
  QAction* pauseAction() const { return act_pause_; }
  QAction* stopAction() const { return act_stop_; }
  QAction* verifyAction() const { return act_verify_; }
  QAction* runAllAction() const { return act_run_all_; }
  QLabel* ribbonStatsLabel() const { return label_ribbon_stats_; }

 signals:
  void engineStateChanged(etest::engine::EngineState state);
  void execStatsUpdated(int pass, int fail, int elapsed);
  void preconditionResult(bool can_run);

 private:
  void connectEngineSignals();
  void refreshMonitorTree();
  bool checkCanVerify() const;
  bool checkCanRun() const;
  /// 加载项目 topology/ 目录下所有 .etopo 到引擎
  void loadProjectTopologies();

  // 引擎
  etest::engine::TestExecutionEngine* engine_ = nullptr;

  // Ribbon QAction（构造时创建，父对象为 MainWindow）
  QAction* act_run_ = nullptr;
  QAction* act_pause_ = nullptr;
  QAction* act_stop_ = nullptr;
  QAction* act_verify_ = nullptr;
  QAction* act_run_all_ = nullptr;
  QLabel* label_ribbon_stats_ = nullptr;

  // 引擎状态
  int pass_count_ = 0;
  int fail_count_ = 0;
  QString current_program_name_;
  QStringList run_queue_;

  // 外部依赖（通过 postInit 注入）
  QWidget* parent_widget_ = nullptr;
  ExecutionDebugWidget* debug_widget_ = nullptr;
  ExecutionOutputPanel* output_panel_ = nullptr;
  etest::core::SignalRegistry* signal_registry_ = nullptr;
  std::shared_ptr<icd::Repository> icd_repository_;
  EditorManager* editor_mgr_ = nullptr;
  TestProgramManagerWidget* test_program_mgr_ = nullptr;
  ProblemsPanel* problems_panel_ = nullptr;
  BottomContainerWidget* bottom_container_ = nullptr;
  AppStatusBarController* status_bar_ctrl_ = nullptr;
  QStackedWidget* central_stack_ = nullptr;
  ExecutionDashboard* dashboard_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_EXECUTIONPANELCONTROLLER_H_
