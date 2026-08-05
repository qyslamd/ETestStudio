#ifndef ETEST_APP_EXECUTIONPANELCONTROLLER_H_
#define ETEST_APP_EXECUTIONPANELCONTROLLER_H_

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>

#include "editors/RunConfig.h"       // 当前运行配置（.erun）数据模型
#include "widgets/ProblemsPanel.h"  // NavTarget 定义（navigateRequested 信号参数）

class QAction;
class QLabel;
class QStackedWidget;
class QWidget;

namespace etest::visualizer {
class SignalVisualizer;
}  // namespace etest::visualizer

namespace etest::app {

class AppStatusBarController;
class MonitorConfigDialog;
class EditorManager;
class ExecutionDebugWidget;
class ExecutionOutputPanel;
class TestProgramManagerWidget;
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
  // 注：test_program_mgr 参数已废弃（程序选择已收敛到 .erun.programs），
  // 保留参数位仅作过渡，后续可一并清理签名。
  // problems_panel/bottom_container 已于阶段三移除。
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

  // 当前运行配置含程序数（MainWindow ribbon 提示用）
  int runProgramCount() const { return run_config_.programs.size(); }

  // Ribbon 动作（供 MainWindow setupRibbon 获取）
  QAction* runAction() const { return act_run_; }
  QAction* pauseAction() const { return act_pause_; }
  QAction* stopAction() const { return act_stop_; }
  QAction* verifyAction() const { return act_verify_; }
  QAction* runAllAction() const { return act_run_all_; }
  QLabel* ribbonStatsLabel() const { return label_ribbon_stats_; }

  // 通道选择 action（供 MainWindow 放入 ribbon）
  QAction* selectChannelsAction() const { return act_select_channels_; }

  /// 弹出监听器配置 Dialog（非模态，决策 18）
  void showChannelSelectionDialog();

  // 清空数据按钮（供 MainWindow 放入 ribbon）
  QAction* clearDataAction() const { return act_clear_data_; }

 signals:
  void engineStateChanged(etest::engine::EngineState state);
  /// 引擎正常执行完成（非手动 stop、非 Error），携带最终 pass/fail 计数
  void engineFinished(int pass, int fail);
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

  /// 左栏选中某连接（右栏高亮由对话框内部处理，此处仅日志）
  // 已废弃（D1-g）：MonitorConfigDialog 交互槽注释，配置收敛到运行编辑器（4.9）
  // void onChannelSelected(const QString& connectionId);
  /// 右栏点类型：已有监听器 → 改 displayMode；无 → 新建（创建即所见）并写回
  // void onVisualizerChosen(const QString& connectionId,
  //                         const QString& displayMode);
  /// checkbox 勾选变化 → 订阅/取消订阅 + 建/撤可视化（不写回，会话内状态）
  // void onCheckToggled(const QString& connectionId, bool checked);
  /// 双击已配置通道重命名主标题 → 同步可视化标题并写回
  // void onRenameRequested(const QString& connectionId, const QString& name);
  /// 删除监听器（含失效）→ 撤可视化并写回
  // void onDeleteRequested(const QString& connectionId);

  /// 将监听器订阅到某个可视化组件（勾选和拓扑重载重订阅复用）
  void subscribeVisualizer(const QString& connectionId,
                           etest::visualizer::SignalVisualizer* vis);
  /// 从 .erun 监听器数组 + 引擎拓扑 JSON 重建监听器（幂等；运行态只读）
  void loadProjectMonitors();
  /// 当前运行配置（.erun）加载 + mtime 检测级联刷新（4.2/4.4）
  void loadCurrentRunConfig();
  /// .erun.programs 相对项目根路径 → 绝对路径（消费前转换，4.6）
  QStringList resolveRunPrograms() const;
  /// 扫描 cases/*.etprog 全部测试程序（runAll 用，绝对路径）
  QStringList scanAllTestPrograms() const;
  /// 按 .erun 重建运行态可视化区（跳过 invalid，应用 layout，4.5）
  void rebuildVisualizers();
  // 已废弃（D1-g）：运行态不再写监听器，写回收敛到运行编辑器
  // void syncProjectMonitorsToFile();
  /// 由拓扑 JSON 构建连接列表（connectionId → device.port ↔ UUT.port 描述）
  QList<QPair<QString, QString>> buildConnectionList() const;
  /// 解析连接对应的设备信息（deviceId/devicePort/deviceType），供 addMonitor
  bool resolveConnection(const QString& connectionId, QString* deviceId,
                         QString* devicePort, QString* deviceType) const;
  /// 连接描述（供监听器默认主标题 / 可视化副标题）
  QString connectionDescription(const QString& connectionId) const;
  /// 创建可视化 + 订阅 + 加入可视化区（创建即所见；已显示则跳过）
  void createAndShowVisualizer(const QString& connectionId,
                               const QString& displayMode);
  /// 重建可视化：按新模式替换（先撤旧、再建新）
  void rebuildVisualizer(const QString& connectionId,
                         const QString& displayMode);

  /// 合并多个测试程序为单个 ProgramData，记录各程序的 case 范围
  etest::engine::ProgramData mergePrograms(const QStringList& paths,
                                            const QString& suiteName);
  /// 单程序报告保存（抽取自 engineFinished lambda）
  void saveSingleReport(const QString& programName);
  /// 分段报告保存（合并运行时按程序切分）
  void saveSegmentReport(const QString& programName,
                         int startCase, int caseCount,
                         const QString& timestamp);

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

  // 合并运行时的程序分段信息（run_segments_ 非空表示合并运行）
  struct ProgramSegment {
    QString name;         // 程序名（报告文件名用）
    int startCaseIndex;   // 合并 ProgramData 中的起始 case 索引
    int caseCount;        // 该程序的 case 数量
  };
  QList<ProgramSegment> run_segments_;

  // topology.etopo 的 mtime 快照（供 syncProjectTopologies 判断变化）
  QDateTime topo_mtime_;

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

  // 监听器配置 Dialog（懒创建并复用，非模态；见 showChannelSelectionDialog）
  MonitorConfigDialog* channel_dialog_ = nullptr;

  // MonitorManager（由 controller 持有，跨引擎重建保持；注入到引擎使用）
  etest::engine::MonitorManager* monitor_manager_ = nullptr;

  // 当前运行配置（.erun）与 mtime 快照（mtime 检测级联刷新，仿 topo_mtime_）
  RunConfig run_config_;
  QString run_config_file_;
  QDateTime run_config_mtime_;
};

}  // namespace etest::app

#endif  // ETEST_APP_EXECUTIONPANELCONTROLLER_H_
