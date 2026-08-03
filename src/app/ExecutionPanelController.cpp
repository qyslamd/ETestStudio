#include "ExecutionPanelController.h"

#include <QAction>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QSet>
#include <QStackedWidget>
#include <QWidget>
#include "ExecutionDashboard.h"
#include "VisualizationArea.h"
#include "dialogs/MonitorConfigDialog.h"
#include "engine/MonitorManager.h"
#include "visualizers/SignalVisualizer.h"
#include "visualizers/VisualizerFactory.h"
#include "visualizers/WaveformWidget.h"

#include "AppIconProvider.h"
#include "AppStatusBarController.h"
#include "EditorManager.h"
#include "ExecutionDebugWidget.h"
#include "ProjectInfo.h"
#include "SignalRegistry.h"
#include "TestProgramManagerWidget.h"
#include "api/IEditor.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "engine/TestExecutionEngine.h"
#include "icd/repository.hpp"
#include "logger/Logger.h"
#include "plugin_sdk/PluginManager.h"
#include "project/ProjectManager.h"
#include "test_program/TestProgramData.h"
#include "test_program/TestProgramEditorWidget.h"
#include "widgets/ExecutionOutputPanel.h"
#include "widgets/ProblemsPanel.h"
#include "widgets/ProgramSelectionPopup.h"

using namespace etest::core::config;
using etest::core_ui::AppIconProvider;

namespace etest::app {

namespace {

// ── 转换工具：TestProgramData → engine::ProgramData ──
etest::engine::TestStepData convertStep(const etest::app::TestStepData& src) {
  etest::engine::TestStepData dst;
  dst.command = src.cmd;
  dst.target = src.target;
  dst.value = src.value.toDouble();
  dst.tolerance = src.tolerance.enabled ? src.tolerance.max : 0.0;
  dst.extra = src.description;
  dst.timeoutMs = src.timeoutMs;
  dst.loopCount = src.loopCount;
  if (src.condition.target.isEmpty()) {
    dst.condition.clear();
  } else {
    dst.condition = src.condition.target + QStringLiteral(" ") +
                    src.condition.op + QStringLiteral(" ") +
                    src.condition.value.toString();
  }
  for (const auto& ss : src.subSteps) {
    dst.subSteps.append(convertStep(ss));
  }
  for (const auto& es : src.elseSubSteps) {
    dst.elseSteps.append(convertStep(es));
  }
  return dst;
}

etest::engine::TestCaseData convertCase(const etest::app::TestCaseData& src) {
  etest::engine::TestCaseData dst;
  dst.caseName = src.name;
  for (const auto& step : src.steps) {
    dst.steps.append(convertStep(step));
  }
  return dst;
}

etest::engine::ProgramData convertProgram(
    const etest::app::TestProgramData& src) {
  etest::engine::ProgramData dst;
  dst.suiteName = src.name;
  for (const auto& tc : src.cases) {
    dst.cases.append(convertCase(tc));
  }
  return dst;
}

}  // anonymous namespace

ExecutionPanelController::ExecutionPanelController(QWidget* parent_widget,
                                                   QObject* parent)
    : QObject(parent), parent_widget_(parent_widget) {
  act_run_ = new QAction(QIcon(), QStringLiteral("运行"), parent_widget_);
  act_pause_ = new QAction(QIcon(), QStringLiteral("暂停"), parent_widget_);
  act_stop_ = new QAction(QIcon(), QStringLiteral("停止"), parent_widget_);
  act_verify_ = new QAction(QIcon(), QStringLiteral("验证"), parent_widget_);
  act_run_all_ =
      new QAction(QIcon(), QStringLiteral("运行全部"), parent_widget_);
  label_ribbon_stats_ =
      new QLabel(QStringLiteral("P 0  F 0  T 0s"), parent_widget_);
  act_clear_data_ =
      new QAction(QIcon(), QStringLiteral("清空数据"), parent_widget_);
  act_clear_data_->setEnabled(false);
  connect(act_clear_data_, &QAction::triggered, this,
          &ExecutionPanelController::clearData);
  popup_ = new ProgramSelectionPopup(parent_widget_);
  connect(popup_, &ProgramSelectionPopup::selectionChanged, this,
          &ExecutionPanelController::updateRunControls);

  act_select_channels_ =
      new QAction(AppIconProvider::instance().icon(QStringLiteral("monitor")),
                  QStringLiteral("通道选择"), parent_widget_);
}

void ExecutionPanelController::postInit(
    ExecutionOutputPanel* output_panel,
    etest::core::SignalRegistry* signal_registry,
    std::shared_ptr<icd::Repository> icd_repository,
    EditorManager* editor_mgr,
    TestProgramManagerWidget* /*test_program_mgr*/,
    AppStatusBarController* status_bar_ctrl) {
  output_panel_ = output_panel;
  signal_registry_ = signal_registry;
  icd_repository_ = std::move(icd_repository);
  editor_mgr_ = editor_mgr;
  status_bar_ctrl_ = status_bar_ctrl;

  // 创建 MonitorManager（由 controller 持有，跨引擎重建保持）
  if (!monitor_manager_) {
    monitor_manager_ = new etest::engine::MonitorManager(this);
  }
}

void ExecutionPanelController::updateIcdContext(
    etest::core::SignalRegistry* signal_registry,
    std::shared_ptr<icd::Repository> icd_repository) {
  signal_registry_ = signal_registry;
  icd_repository_ = std::move(icd_repository);
}

void ExecutionPanelController::setCentralStack(QStackedWidget* stack) {
  central_stack_ = stack;
}

void ExecutionPanelController::createEngine() {
  if (engine_) {
    return;
  }
  if (!signal_registry_ || !icd_repository_) {
    return;
  }
  engine_ = new etest::engine::TestExecutionEngine(signal_registry_,
                                                   icd_repository_.get(), this);
  // 注入外部 MonitorManager（由 controller 持有，引擎重建时保持）
  engine_->setMonitorManager(monitor_manager_);
  connectEngineSignals();
}

void ExecutionPanelController::destroyEngine() {
  // 所有编辑器恢复编辑状态（不自动切回编辑态）
  if (editor_mgr_) {
    for (auto* ed : editor_mgr_->allEditors()) {
      ed->setReadOnly(false);
    }
  }
  if (!engine_) {
    return;
  }
  engine_->stop();
  engine_->deleteLater();
  engine_ = nullptr;
}

void ExecutionPanelController::connectEngineSignals() {
  if (!engine_) {
    return;
  }

  // 引擎状态 → ribbon 同步 + statusbar
  connect(engine_, &etest::engine::TestExecutionEngine::engineStateChanged,
          this, [this](etest::engine::EngineState state) {
            syncControlStates();
            QString state_text;
            switch (state) {
              case etest::engine::EngineState::Idle:
                state_text = QStringLiteral("就绪");
                break;
              case etest::engine::EngineState::Running:
                state_text = QStringLiteral("运行中");
                break;
              case etest::engine::EngineState::Paused:
                state_text = QStringLiteral("已暂停");
                break;
              case etest::engine::EngineState::Finished:
                state_text = QStringLiteral("已完成 (P%1 F%2)")
                                 .arg(pass_count_)
                                 .arg(fail_count_);
                break;
              case etest::engine::EngineState::Error:
                state_text = QStringLiteral("错误");
                break;
            }
            if (status_bar_ctrl_) {
              status_bar_ctrl_->setEngineState(state_text);
            }
            emit engineStateChanged(state);
          });

  // 绑定调试面板
  if (debug_widget_) {
    debug_widget_->bindEngine(engine_);
  }

  // suiteFinished → 累计统计
  connect(engine_, &etest::engine::TestExecutionEngine::suiteFinished, this,
          [this](const QString& /*name*/, int pass, int fail) {
            pass_count_ += pass;
            fail_count_ += fail;
            if (status_bar_ctrl_) {
              status_bar_ctrl_->setExecStats(pass_count_, fail_count_, 0);
            }
            emit execStatsUpdated(pass_count_, fail_count_, 0);
          });

  // 步骤结果 → 输出面板
  connect(engine_, &etest::engine::TestExecutionEngine::stepFinished, this,
          [this](int /*case_index*/, const QString& /*step_path*/,
                 const etest::engine::StepResult& result) {
            if (output_panel_) {
              output_panel_->appendResult(result);
            }
          });

  // 引擎级错误 → 输出面板
  connect(engine_, &etest::engine::TestExecutionEngine::engineError, this,
          [this](const QString& msg) {
            if (output_panel_) {
              output_panel_->appendError(msg);
            }
          });

  // 引擎完成 → 保存 .etlog 报告（不自动切回编辑态）
  connect(engine_, &etest::engine::TestExecutionEngine::engineFinished, this,
          [this]() {
            if (editor_mgr_) {
              for (auto* ed : editor_mgr_->allEditors()) {
                ed->setReadOnly(false);
              }
            }
            if (current_program_name_.isEmpty()) {
              return;
            }
            if (run_segments_.isEmpty()) {
              // 单程序运行：保存单个报告
              saveSingleReport(current_program_name_);
            } else {
              // 合并运行：一次性 flush monitor 数据，再按程序分段保存
              engine_->flushMonitorData();
              QString timestamp =
                  QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
              for (const auto& seg : run_segments_) {
                saveSegmentReport(seg.name, seg.startCaseIndex, seg.caseCount,
                                  timestamp);
              }
              run_segments_.clear();
            }
            // 通知外部：引擎正常完成，携带 pass/fail 计数
            emit engineFinished(pass_count_, fail_count_);
          });
}

void ExecutionPanelController::syncControlStates() {
  // 刷新 popup 程序列表，确保状态检查时列表已填充
  // refreshList() 保留已有选中状态（selected_），不会丢失用户选择
  if (popup_ &&
      etest::core::project::ProjectManager::instance().isProjectOpen()) {
    popup_->refreshList();
  }

  if (!engine_) {
    act_run_->setEnabled(checkCanRun());
    act_run_all_->setEnabled(checkCanRunAll());
    act_pause_->setEnabled(false);
    act_stop_->setEnabled(false);
    act_verify_->setEnabled(checkCanVerify());
    act_clear_data_->setEnabled(false);
    bool projectOpen =
        etest::core::project::ProjectManager::instance().isProjectOpen();
    popup_->setEnabled(projectOpen);
    act_select_channels_->setEnabled(projectOpen);
    return;
  }
  auto state = engine_->state();
  switch (state) {
    case etest::engine::EngineState::Idle:
    case etest::engine::EngineState::Finished:
      act_run_->setEnabled(checkCanRun());
      act_run_all_->setEnabled(checkCanRunAll());
      act_pause_->setEnabled(false);
      act_stop_->setEnabled(false);
      act_verify_->setEnabled(checkCanVerify());
      act_clear_data_->setEnabled(true);
      popup_->setEnabled(true);
      act_select_channels_->setEnabled(true);
      break;
    case etest::engine::EngineState::Running:
      act_run_->setEnabled(false);
      act_run_all_->setEnabled(false);
      act_pause_->setEnabled(true);
      act_pause_->setText(QStringLiteral("暂停"));
      act_pause_->setIcon(
          AppIconProvider::instance().icon(QStringLiteral("pause")));
      act_stop_->setEnabled(true);
      act_verify_->setEnabled(false);
      act_clear_data_->setEnabled(false);
      popup_->setEnabled(false);
      act_select_channels_->setEnabled(false);
      break;
    case etest::engine::EngineState::Paused:
      act_run_->setEnabled(false);
      act_run_all_->setEnabled(false);
      act_pause_->setEnabled(true);
      act_pause_->setText(QStringLiteral("继续"));
      act_pause_->setIcon(
          AppIconProvider::instance().icon(QStringLiteral("resume")));
      act_stop_->setEnabled(true);
      act_verify_->setEnabled(false);
      act_clear_data_->setEnabled(false);
      popup_->setEnabled(false);
      act_select_channels_->setEnabled(false);
      break;
    case etest::engine::EngineState::Error:
      act_run_->setEnabled(checkCanRun());
      act_run_all_->setEnabled(checkCanRunAll());
      act_pause_->setEnabled(false);
      act_stop_->setEnabled(false);
      act_verify_->setEnabled(checkCanVerify());
      act_clear_data_->setEnabled(true);
      popup_->setEnabled(true);
      act_select_channels_->setEnabled(true);
      break;
  }
}

bool ExecutionPanelController::checkCanVerify() const {
  bool ok = etest::core::project::ProjectManager::instance().isProjectOpen();
  LOG_DEBUG("ENABLE", "checkCanVerify={}", ok);
  return ok;
}

bool ExecutionPanelController::checkCanRunAll() const {
  if (!checkCanVerify()) {
    LOG_DEBUG("ENABLE", "checkCanRunAll=false (checkCanVerify)");
    return false;
  }
  if (!debug_widget_ || !debug_widget_->canRun()) {
    LOG_DEBUG("ENABLE", "checkCanRunAll=false (canRun={})",
              debug_widget_ ? debug_widget_->canRun() : false);
    return false;
  }
  bool has = popup_ && popup_->hasAnyProgram();
  LOG_DEBUG("ENABLE", "checkCanRunAll={}", has);
  return has;
}

bool ExecutionPanelController::checkCanRun() const {
  if (!checkCanVerify()) {
    LOG_DEBUG("ENABLE", "checkCanRun=false (checkCanVerify)");
    return false;
  }
  if (!debug_widget_ || !debug_widget_->canRun()) {
    LOG_DEBUG("ENABLE", "checkCanRun=false (canRun={})",
              debug_widget_ ? debug_widget_->canRun() : false);
    return false;
  }
  bool sel = popup_ && !popup_->selectedPaths().isEmpty();
  LOG_DEBUG("ENABLE", "checkCanRun={}", sel);
  return sel;
}

void ExecutionPanelController::updateRunControls() {
  if (engine_ && (engine_->state() == etest::engine::EngineState::Running ||
                  engine_->state() == etest::engine::EngineState::Paused)) {
    // 引擎运行时 run/runAll 始终禁用，不检查预条件
    return;
  }
  act_run_->setEnabled(checkCanRun());
  act_run_all_->setEnabled(checkCanRunAll());
  // popup 从空到有数据时 verify 状态可能变化，一并刷新
  act_verify_->setEnabled(checkCanVerify());
}

// 清空 MonitorManager
// 的结构（lookup_table_/tree_cache_/subscribers_）与运行时数据（buffer_）。
// 注意：clearStructure 会清空 subscribers_，dashboard
// 中的可视化组件将收不到后续采样，
// 用户需重新勾选通道恢复订阅。此为已知权衡（拓扑变化属低频场景，订阅恢复逻辑脆弱已移除）。
void ExecutionPanelController::clearMonitorState() {
  if (monitor_manager_) {
    monitor_manager_->clearStructure();
    monitor_manager_->clearRuntime();
  }
}

void ExecutionPanelController::syncProjectTopologies() {
  if (!engine_) {
    return;
  }

  auto& proj_mgr = etest::core::project::ProjectManager::instance();
  if (!proj_mgr.isProjectOpen()) {
    return;
  }
  QString topo_dir =
      proj_mgr.currentProjectRoot() + QStringLiteral("/topology");
  QDir topo_dir_obj(topo_dir);
  if (!topo_dir_obj.exists()) {
    LOG_WARN("ENGINE", "拓扑目录不存在: {}", topo_dir.toStdString());
    return;
  }
  // 单拓扑约束：只加载 topology/topology.etopo
  QString topo_path =
      topo_dir_obj.absoluteFilePath(QStringLiteral("topology.etopo"));
  QFileInfo topo_fi(topo_path);

  auto* mm = monitor_manager_;
  if (!mm) {
    return;
  }

  // ── 检测多余 .etopo 文件并警告 ──
  const auto extra_files = topo_dir_obj.entryInfoList(
      {QStringLiteral("*.etopo")}, QDir::Files, QDir::Name);
  for (const auto& fi : extra_files) {
    if (fi.fileName() != QStringLiteral("topology.etopo")) {
      LOG_WARN("ENGINE", "检测到多余 .etopo 文件，已忽略: {}",
               fi.fileName().toStdString());
    }
  }

  // ── topology.etopo 不存在 ──
  if (!topo_fi.exists()) {
    if (!extra_files.isEmpty()) {
      LOG_WARN("ENGINE", "topology.etopo 不存在，目录下有其他 .etopo 文件，"
               "请手动重命名为 topology.etopo");
    } else {
      LOG_WARN("ENGINE", "拓扑文件缺失: topology.etopo 不存在，请创建拓扑");
    }
    return;
  }

  QDateTime current_mtime = topo_fi.lastModified();
  bool tree_empty = mm->monitorTree().isEmpty();

  if (tree_empty) {
    // ── 首次加载 / 引擎重建后 ──
    bool ok = engine_->loadTopology(topo_path);
    if (!ok) {
      LOG_ERROR("ENGINE", "拓扑文件加载失败: {}",
                topo_path.toStdString());
      return;
    }
    topo_mtime_ = current_mtime;
    loadProjectMonitors();
    refreshMonitorTree();
    return;
  }

  // ── mtime 未变：跳过 ──
  if (topo_mtime_.isValid() && topo_mtime_ == current_mtime) {
    return;
  }

  // ── mtime 变了：全量重建 ──
  // clearMonitorState() 仍在 loadTopology 之前调用：loadTopology 有多个早返回路径
  // （文件损坏、JSON 解析失败等）不会继续；监听器由 loadProjectMonitors 从 .etproj 加载
  engine_->clearTopologyState();
  clearMonitorState();
  bool ok = engine_->loadTopology(topo_path);
  if (!ok) {
    LOG_ERROR("ENGINE", "拓扑文件加载失败: {}",
              topo_path.toStdString());
    return;
  }
  topo_mtime_ = current_mtime;
  loadProjectMonitors();
  refreshMonitorTree();

  // ── 决策 13：拓扑重载后自动重订阅 ──
  // loadProjectMonitors 已清空 subscribers_，此处对仍活跃的可视化组件重新订阅；
  // 失效监听器（连接已删）移除对应可视化（决策 6：可见可处理，不静默）。
  auto* visArea = dashboard_ ? dashboard_->visualizationArea() : nullptr;
  if (visArea) {
    QSet<QString> invalid;
    const auto tree = mm->monitorTree();
    for (const auto& entry : tree) {
      if (entry.invalid) {
        invalid.insert(entry.connectionId);
      }
    }
    const QList<QString> channels = visArea->activeChannels();
    for (const QString& cid : channels) {
      if (invalid.contains(cid)) {
        mm->unsubscribe(cid);
        visArea->removeVisualizer(cid);
        continue;
      }
      SignalVisualizer* vis = visArea->visualizer(cid);
      if (vis) {
        subscribeVisualizer(cid, vis);
      }
    }
  }
}

void ExecutionPanelController::run() {
  LOG_INFO("MAIN_UI", "点击「运行」");

  if (!popup_) {
    return;
  }

  // 1. 从 popup 获取选中目标
  QStringList paths = popup_->selectedPaths();
  if (paths.isEmpty()) {
    QMessageBox::information(parent_widget_, QStringLiteral("运行"),
                             QStringLiteral("请先选择要运行的测试程序"));
    return;
  }

  // 2. 前提检查
  if (debug_widget_ && !debug_widget_->canRun()) {
    QMessageBox::warning(parent_widget_, QStringLiteral("运行"),
                         QStringLiteral("运行前提不满足，请先执行验证"));
    return;
  }

  // 3. 未保存提示（队列启动时一次性扫描）
  if (!checkUnsavedAndPrompt(paths)) {
    return;
  }

  // 4. 选中 >=2 个 -> 合并执行（不重建引擎）
  if (paths.size() > 1) {
    etest::engine::ProgramData merged =
        mergePrograms(paths, QStringLiteral("选中程序"));
    if (merged.cases.isEmpty()) {
      QMessageBox::warning(parent_widget_, QStringLiteral("运行"),
                           QStringLiteral("选中的程序中没有可用测试用例"));
      return;
    }

    createEngine();
    if (!engine_) {
      return;
    }

    syncProjectTopologies();

    current_program_name_ = QStringLiteral("选中程序");
    engine_->setProgram(merged);

    pass_count_ = 0;
    fail_count_ = 0;
    if (status_bar_ctrl_) {
      status_bar_ctrl_->setExecStats(0, 0, 0);
    }

    engine_->start();
    if (dashboard_) {
      dashboard_->setCurrentBottomTab(0);
    }
    if (editor_mgr_) {
      for (auto* ed : editor_mgr_->allEditors()) {
        ed->setReadOnly(true);
      }
    }
    if (central_stack_) {
      central_stack_->setCurrentIndex(1);
    }
    return;
  }

  // 5. 单程序直接执行
  etest::app::TestProgramData data = loadTestProgram(paths[0]);
  if (data.name.isEmpty() || data.cases.isEmpty()) {
    QMessageBox::warning(parent_widget_, QStringLiteral("运行"),
                         QStringLiteral("测试程序中没有测试用例，无法运行"));
    return;
  }

  createEngine();
  if (!engine_) {
    return;
  }

  // 同步拓扑数据（mtime 没变则跳过）
  syncProjectTopologies();

  // 设置程序数据
  current_program_name_ = data.name;
  engine_->setProgram(convertProgram(data));

  // 重置统计
  pass_count_ = 0;
  fail_count_ = 0;
  if (status_bar_ctrl_) {
    status_bar_ctrl_->setExecStats(0, 0, 0);
  }

  // 启动 + 切换到运行态
  engine_->start();
  // 切到「输出」tab（用户点运行应看到运行日志）
  if (dashboard_) {
    dashboard_->setCurrentBottomTab(0);
  }
  // 所有编辑器置为只读
  if (editor_mgr_) {
    for (auto* ed : editor_mgr_->allEditors()) {
      ed->setReadOnly(true);
    }
  }
  if (central_stack_) {
    central_stack_->setCurrentIndex(1);
  }
}

void ExecutionPanelController::pause() {
  if (!engine_) {
    return;
  }
  if (engine_->state() == etest::engine::EngineState::Running) {
    LOG_INFO("MAIN_UI", "点击「暂停」");
    engine_->pause();
  } else if (engine_->state() == etest::engine::EngineState::Paused) {
    LOG_INFO("MAIN_UI", "点击「继续」");
    engine_->resume();
  }
}

void ExecutionPanelController::stop() {
  LOG_INFO("MAIN_UI", "点击「停止」");
  run_segments_
      .clear();  // 清空分段，防 engineFinished 尝试切片未执行完的 cases
  if (engine_) {
    engine_->stop();
  }
  // 恢复编辑器编辑状态（不自动切回编辑态，由用户通过 QAB 手工操作）
  if (editor_mgr_) {
    for (auto* ed : editor_mgr_->allEditors()) {
      ed->setReadOnly(false);
    }
  }
}

void ExecutionPanelController::verify() {
  LOG_INFO("MAIN_UI", "点击「校验」");
  if (!dashboard_) {
    return;
  }
  auto* problems = dashboard_->problemsPanel();
  if (!problems) {
    return;
  }
  problems->clearProblems();
  int errors = 0;
  int warnings = 0;

  // 1. 项目已打开
  auto& proj_mgr = etest::core::project::ProjectManager::instance();
  bool project_open = proj_mgr.isProjectOpen();
  if (!project_open) {
    problems->addProblem(NavTarget::Project, QStringLiteral("项目"),
                         QStringLiteral("错误"), QStringLiteral("未打开项目"));
    errors++;
  }

  // 2. ICD 协议已定义
  bool icd_loaded = icd_repository_ && !icd_repository_->frames().empty();
  if (!icd_loaded) {
    problems->addProblem(NavTarget::Icd, QStringLiteral("ICD 协议"),
                         QStringLiteral("错误"),
                         QStringLiteral("ICD 协议未加载"));
    errors++;
  }

  // 3. 拓扑已配置
  bool topo_exists = false;
  if (project_open) {
    QString topo_dir =
        proj_mgr.currentProjectRoot() + QStringLiteral("/topology");
    QDir topo_dir_obj(topo_dir);
    topo_exists =
        topo_dir_obj.exists() &&
        !topo_dir_obj.entryList({QStringLiteral("*.etopo")}, QDir::Files)
             .isEmpty();
  }
  if (!topo_exists) {
    problems->addProblem(NavTarget::Topology, QStringLiteral("拓扑"),
                         QStringLiteral("错误"),
                         QStringLiteral("未找到拓扑文件"));
    errors++;
  }

  // 4. 拓扑已绑定信号
  bool signal_bound =
      signal_registry_ && !signal_registry_->registeredDeviceIds().isEmpty();
  if (!signal_bound) {
    problems->addProblem(NavTarget::Signal, QStringLiteral("拓扑"),
                         QStringLiteral("警告"),
                         QStringLiteral("拓扑未绑定信号"));
    warnings++;
  }

  // 5. 测试程序可用（方案 A：选中非空校验选中，空则校验全集）
  bool has_program = false;
  if (popup_) {
    QStringList targets = popup_->selectedPaths();
    if (targets.isEmpty()) {
      targets = popup_->allPaths();
    }
    for (const auto& p : targets) {
      auto data = loadTestProgram(p);
      if (!data.cases.isEmpty()) {
        has_program = true;
        break;
      }
    }
  }
  if (!has_program) {
    problems->addProblem(NavTarget::Program, QStringLiteral("测试程序"),
                         QStringLiteral("错误"),
                         QStringLiteral("无可用测试程序"));
    errors++;
  }

  // 6. 硬件/Mock 状态
  if (!topo_exists && !signal_bound) {
    problems->addProblem(NavTarget::Hardware, QStringLiteral("硬件"),
                         QStringLiteral("警告"),
                         QStringLiteral("硬件/Mock 未配置"));
    warnings++;
  }

  problems->showSummary(errors, warnings);
  LOG_INFO("MAIN_UI", "校验完成 [errors={}, warnings={}]", errors, warnings);

  // 有问题时自动切到 page1 底部「问题」tab
  if (errors > 0 || warnings > 0) {
    dashboard_->showProblemsTab();
  }

  // 刷新执行调试面板
  if (debug_widget_) {
    debug_widget_->setDependencies(signal_registry_, icd_repository_.get());
  }

  // verify 可能改变了 canRun 状态，同步 ribbon 按钮 enable
  syncControlStates();
}

void ExecutionPanelController::runAll() {
  LOG_INFO("MAIN_UI", "点击「运行全部」");

  if (!popup_) {
    return;
  }

  // 1. 前提检查
  if (debug_widget_ && !debug_widget_->canRun()) {
    QMessageBox::warning(parent_widget_, QStringLiteral("运行全部"),
                         QStringLiteral("运行前提不满足，请先执行验证"));
    return;
  }

  // 2. 从 popup 获取全集（内部扫描 cases/*.etprog）
  popup_->refreshList();
  QStringList all_paths = popup_->allPaths();
  if (all_paths.isEmpty()) {
    QMessageBox::information(parent_widget_, QStringLiteral("运行全部"),
                             QStringLiteral("项目中没有测试程序"));
    return;
  }

  if (!checkUnsavedAndPrompt(all_paths)) {
    return;
  }

  // 4. 合并所有程序为单个 ProgramData 执行（不重建引擎）
  etest::engine::ProgramData merged =
      mergePrograms(all_paths, QStringLiteral("全部程序"));
  if (merged.cases.isEmpty()) {
    QMessageBox::warning(parent_widget_, QStringLiteral("运行全部"),
                         QStringLiteral("没有可用的测试程序"));
    return;
  }

  createEngine();
  if (!engine_) {
    return;
  }

  syncProjectTopologies();

  current_program_name_ = QStringLiteral("全部程序");
  engine_->setProgram(merged);

  pass_count_ = 0;
  fail_count_ = 0;
  if (status_bar_ctrl_) {
    status_bar_ctrl_->setExecStats(0, 0, 0);
  }

  engine_->start();
  if (dashboard_) {
    dashboard_->setCurrentBottomTab(0);
  }
  if (editor_mgr_) {
    for (auto* ed : editor_mgr_->allEditors()) {
      ed->setReadOnly(true);
    }
  }
  if (central_stack_) {
    central_stack_->setCurrentIndex(1);
  }
}

bool ExecutionPanelController::checkUnsavedAndPrompt(
    const QStringList& paths) const {
  if (!editor_mgr_) {
    return true;
  }

  // 找出与运行目标匹配且有未保存修改的编辑器
  QStringList unsavedFiles;
  for (auto* editor : editor_mgr_->allEditors()) {
    if (editor->isModified() && paths.contains(editor->filePath())) {
      unsavedFiles.append(editor->filePath());
    }
  }

  if (unsavedFiles.isEmpty()) {
    return true;  // 无未保存，直接跑
  }

  // 弹模态确认框
  QString msg = QStringLiteral("以下测试程序有未保存的修改：\n\n");
  for (const auto& f : unsavedFiles) {
    msg +=
        QStringLiteral("  ") + QFileInfo(f).fileName() + QStringLiteral("\n");
  }
  msg += QStringLiteral("\n请选择操作：");

  QMessageBox box(parent_widget_);
  box.setWindowTitle(QStringLiteral("运行前保存"));
  box.setText(msg);
  auto* saveBtn =
      box.addButton(QStringLiteral("保存并运行"), QMessageBox::AcceptRole);
  auto* runBtn =
      box.addButton(QStringLiteral("不保存运行"), QMessageBox::DestructiveRole);
  box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
  box.exec();

  QAbstractButton* clicked = box.clickedButton();
  if (clicked == saveBtn) {
    // 只保存属于运行目标且未保存的文件（unsavedFiles），不碰其他编辑器
    for (const auto& f : unsavedFiles) {
      // 从 editor_mgr_ 中查找对应路径的编辑器
      for (auto* editor : editor_mgr_->allEditors()) {
        if (editor->filePath() == f && editor->isModified()) {
          if (!editor->save()) {
            QMessageBox::warning(const_cast<QWidget*>(parent_widget_),
                                 QStringLiteral("保存失败"),
                                 QStringLiteral("无法保存文件: %1").arg(f));
            return false;
          }
          break;
        }
      }
    }
    return true;
  }
  if (clicked == runBtn) {
    return true;  // 不保存，跑磁盘内容
  }
  return false;  // 取消
}

etest::engine::ProgramData ExecutionPanelController::mergePrograms(
    const QStringList& paths,
    const QString& suiteName) {
  run_segments_.clear();

  etest::engine::ProgramData merged;
  merged.suiteName = suiteName;

  int caseOffset = 0;
  for (const QString& path : paths) {
    etest::app::TestProgramData data = loadTestProgram(path);
    if (data.name.isEmpty() || data.cases.isEmpty()) {
      LOG_WARN("MAIN_UI", "跳过空或加载失败的程序 [path={}]",
               path.toStdString());
      continue;
    }

    etest::engine::ProgramData prog = convertProgram(data);
    merged.cases.append(prog.cases);
    run_segments_.append({data.name, caseOffset, prog.cases.size()});
    caseOffset += prog.cases.size();

    LOG_INFO("MAIN_UI", "合并程序 [name={} cases={}]", data.name.toStdString(),
             prog.cases.size());
  }

  return merged;
}

void ExecutionPanelController::saveSingleReport(const QString& programName) {
  auto& proj_mgr = etest::core::project::ProjectManager::instance();
  QString report_dir =
      proj_mgr.currentProjectRoot() + QStringLiteral("/reports");
  QDir().mkpath(report_dir);
  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
  QString etlog_path = report_dir + QStringLiteral("/") + programName +
                       QStringLiteral("_") + timestamp +
                       QStringLiteral(".etlog");
  engine_->saveReport(etlog_path);
}

void ExecutionPanelController::saveSegmentReport(const QString& programName,
                                                 int startCase,
                                                 int caseCount,
                                                 const QString& timestamp) {
  auto& proj_mgr = etest::core::project::ProjectManager::instance();
  QString report_dir =
      proj_mgr.currentProjectRoot() + QStringLiteral("/reports");
  QDir().mkpath(report_dir);
  QString etlog_path = report_dir + QStringLiteral("/") + programName +
                       QStringLiteral("_") + timestamp +
                       QStringLiteral(".etlog");
  engine_->saveReportSegment(etlog_path, programName, startCase, caseCount);
}  // ══════════════════════════════════════════════════════════════════════════════
// setDashboard — 注入执行仪表盘并连接监听器信号
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionPanelController::setDashboard(ExecutionDashboard* dashboard) {
  dashboard_ = dashboard;
  if (!dashboard_) {
    return;
  }
  debug_widget_ = dashboard_->debugWidget();

  if (auto* problems = dashboard_->problemsPanel()) {
    connect(problems, &ProblemsPanel::problemActivated, this,
            [this](NavTarget target) { emit navigateRequested(target); });
  }

  // ── 可视化区右键关闭 → 取消订阅 + 同步对话框 checkbox（取消勾选） ──
  // setChecked 用 QSignalBlocker 不会回触发 checkToggled，故这里必须补 unsubscribe，
  // 否则订阅回调残留在 subscribers_（审查 🟡B）。
  connect(dashboard_->visualizationArea(), &VisualizationArea::visualizerClosed,
          this, [this](const QString& connectionId) {
            if (monitor_manager_) {
              monitor_manager_->unsubscribe(connectionId);
            }
            if (channel_dialog_ && dashboard_) {
              channel_dialog_->setChecked(
                  dashboard_->visualizationArea()->activeChannels());
            }
          });

  // ── 如果引擎已就绪，立即加载监听器树 ──
  refreshMonitorTree();
}

void ExecutionPanelController::refreshMonitorTree() {
  auto* mm = monitor_manager_;
  if (!mm || !channel_dialog_) {
    return;
  }
  channel_dialog_->setConnections(buildConnectionList());
  channel_dialog_->setMonitors(mm->monitorTree());
  channel_dialog_->setChecked(
      dashboard_ ? dashboard_->visualizationArea()->activeChannels()
                 : QList<QString>());
}

// ══════════════════════════════════════════════════════════════════════════════
// clearData — 清空所有可视化组件的采样数据（Ribbon 按钮触发）
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionPanelController::clearData() {
  if (!dashboard_) {
    return;
  }
  auto* visArea = dashboard_->visualizationArea();
  QList<QString> channels = visArea->activeChannels();
  for (const QString& cid : channels) {
    SignalVisualizer* vis = visArea->visualizer(cid);
    if (vis) {
      vis->clearData();
    }
  }
  // 清空左侧执行进度树
  if (debug_widget_) {
    debug_widget_->clear();
  }
  // 清空 CVT buffer_（波形归零），保留 history_buffer_（报告不受影响）
  if (monitor_manager_) {
    monitor_manager_->clearData();
  }
  LOG_INFO("MAIN_UI", "清空数据 [channels={}]", channels.size());
}

// ══════════════════════════════════════════════════════════════════════════════
// clearProjectState — 清理与当前项目相关的监听器状态（项目关闭时调用）
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionPanelController::clearProjectState() {
  // 清理 UI
  if (dashboard_) {
    // 先清可视化组件（clearAll 发射 visualizerClosed → setChecked 同步勾选态）
    dashboard_->visualizationArea()->clearAll();
  }
  // 非模态对话框随项目关闭隐藏（决策 18：不常驻）
  if (channel_dialog_) {
    channel_dialog_->close();
  }
  // 清理 MonitorManager 结构与运行时数据（项目切换时避免残留）
  clearMonitorState();
  // 清理 mtime 缓存
  topo_mtime_ = QDateTime();
}

void ExecutionPanelController::showChannelSelectionDialog() {
  if (!channel_dialog_) {
    channel_dialog_ = new MonitorConfigDialog(parent_widget_);
    connect(channel_dialog_, &MonitorConfigDialog::channelSelected, this,
            &ExecutionPanelController::onChannelSelected);
    connect(channel_dialog_, &MonitorConfigDialog::visualizerChosen, this,
            &ExecutionPanelController::onVisualizerChosen);
    connect(channel_dialog_, &MonitorConfigDialog::checkToggled, this,
            &ExecutionPanelController::onCheckToggled);
    connect(channel_dialog_, &MonitorConfigDialog::renameRequested, this,
            &ExecutionPanelController::onRenameRequested);
    connect(channel_dialog_, &MonitorConfigDialog::deleteRequested, this,
            &ExecutionPanelController::onDeleteRequested);
    refreshMonitorTree();
  }
  // 决策 18：非模态，与执行页并存，配置后立即看到可视化区变化
  channel_dialog_->show();
  channel_dialog_->raise();
  channel_dialog_->activateWindow();
}

void ExecutionPanelController::onChannelSelected(const QString& connectionId) {
  LOG_DEBUG("VISUAL", "channelSelected connectionId={}",
            connectionId.toStdString());
}

void ExecutionPanelController::onVisualizerChosen(
    const QString& connectionId, const QString& displayMode) {
  LOG_INFO("VISUAL", "visualizerChosen cid={} mode={}",
           connectionId.toStdString(), displayMode.toStdString());
  auto* mm = monitor_manager_;
  if (!mm || !engine_) {
    return;
  }
  if (connectionId.isEmpty() || displayMode.isEmpty()) {
    return;
  }

  // 已有监听器 → 改 displayMode（决策 15：点类型 = 切换）
  bool existing = false;
  bool invalid = false;
  for (const auto& entry : mm->monitorTree()) {
    if (entry.connectionId == connectionId) {
      existing = true;
      invalid = entry.invalid;
      break;
    }
  }
  if (existing) {
    if (invalid) {
      return;  // 失效监听器仅可删除（决策 6/17）
    }
    if (mm->setDisplayMode(connectionId, displayMode)) {
      rebuildVisualizer(connectionId, displayMode);
      syncProjectMonitorsToFile();
      refreshMonitorTree();
    }
    return;
  }

  // 无 → 新建（创建即所见），key = connectionId（一连接一监听器，决策 17）
  QString deviceId;
  QString devicePort;
  QString deviceType;
  if (!resolveConnection(connectionId, &deviceId, &devicePort, &deviceType)) {
    LOG_WARN("VISUAL", "无法解析连接 {} 的设备信息，忽略创建",
             connectionId.toStdString());
    return;
  }
  etest::engine::MonitorConfig config;
  config.connectionId = connectionId;
  config.name = connectionDescription(connectionId);
  config.displayMode = displayMode;
  if (!mm->addMonitor(config, deviceId, devicePort, deviceType)) {
    LOG_WARN("VISUAL", "addMonitor 失败 cid={} mode={}",
             connectionId.toStdString(), displayMode.toStdString());
    return;
  }
  createAndShowVisualizer(connectionId, displayMode);
  syncProjectMonitorsToFile();
  refreshMonitorTree();
}

void ExecutionPanelController::onCheckToggled(const QString& connectionId,
                                              bool checked) {
  LOG_INFO("VISUAL", "checkToggled connectionId={} checked={}",
           connectionId.toStdString(), checked);
  auto* mm = monitor_manager_;
  if (!mm) {
    return;
  }
  if (checked) {
    const QString mode = mm->displayMode(connectionId);
    if (mode.isEmpty()) {
      return;  // 未配置监听器的连接不会出现 checkbox，防御性返回
    }
    createAndShowVisualizer(connectionId, mode);
  } else {
    if (dashboard_) {
      dashboard_->visualizationArea()->removeVisualizer(connectionId);
    }
    mm->unsubscribe(connectionId);
  }
}

void ExecutionPanelController::onRenameRequested(const QString& connectionId,
                                                 const QString& name) {
  auto* mm = monitor_manager_;
  if (!mm) {
    return;
  }
  if (!mm->renameMonitor(connectionId, name)) {
    return;
  }
  // 同步可视化主标题（决策 14 两级标题）
  if (dashboard_) {
    SignalVisualizer* vis =
        dashboard_->visualizationArea()->visualizer(connectionId);
    if (vis) {
      vis->setTitle(name);
    }
  }
  syncProjectMonitorsToFile();
  refreshMonitorTree();
}

void ExecutionPanelController::onDeleteRequested(const QString& connectionId) {
  auto* mm = monitor_manager_;
  if (!mm) {
    return;
  }
  if (!mm->removeMonitor(connectionId)) {
    return;
  }
  if (dashboard_) {
    dashboard_->visualizationArea()->removeVisualizer(connectionId);
  }
  syncProjectMonitorsToFile();
  refreshMonitorTree();
}

// ══════════════════════════════════════════════════════════════════════════════
// createAndShowVisualizer — 创建可视化 + 订阅 + 加入可视化区（创建即所见）
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionPanelController::createAndShowVisualizer(
    const QString& connectionId, const QString& displayMode) {
  auto* visArea = dashboard_ ? dashboard_->visualizationArea() : nullptr;
  if (!visArea || !monitor_manager_) {
    return;
  }
  if (visArea->visualizer(connectionId)) {
    return;  // 已显示
  }
  QString name = connectionDescription(connectionId);
  for (const auto& entry : monitor_manager_->monitorTree()) {
    if (entry.connectionId == connectionId) {
      name = entry.name;
      break;
    }
  }
  SignalVisualizer* vis =
      createVisualizerFor(connectionId, displayMode, QString(), name, nullptr);
  if (!vis) {
    return;
  }
  if (auto* wave = qobject_cast<WaveformWidget*>(vis)) {
    static const QColor kColors[] = {
        QColor(0, 120, 215),  QColor(229, 57, 53),
        QColor(67, 160, 71),  QColor(255, 152, 0),
        QColor(156, 39, 176), QColor(0, 151, 167),
        QColor(121, 85, 72),  QColor(158, 158, 158)};
    wave->addTrace(connectionId, kColors[qHash(connectionId) % 8]);
  }
  // 二级标题 = 连接描述（决策 14）
  vis->setSubtitle(connectionDescription(connectionId));
  LOG_INFO("ENGINE", "创建可视化 connectionId={} mode={} type={}",
           connectionId.toStdString(), displayMode.toStdString(),
           vis->metaObject()->className());
  subscribeVisualizer(connectionId, vis);
  visArea->addVisualizer(connectionId, vis);
}

// ══════════════════════════════════════════════════════════════════════════════
// rebuildVisualizer — 按新模式重建可视化（先撤旧、再建新）
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionPanelController::rebuildVisualizer(const QString& connectionId,
                                                 const QString& displayMode) {
  auto* visArea = dashboard_ ? dashboard_->visualizationArea() : nullptr;
  if (!visArea) {
    return;
  }
  if (visArea->visualizer(connectionId)) {
    if (monitor_manager_) {
      monitor_manager_->unsubscribe(connectionId);
    }
    visArea->removeVisualizer(connectionId);
  }
  createAndShowVisualizer(connectionId, displayMode);
}

// ══════════════════════════════════════════════════════════════════════════════
// 连接信息工具（由拓扑 JSON 解析，供对话框列表 / 监听器创建）
// ══════════════════════════════════════════════════════════════════════════════

QList<QPair<QString, QString>> ExecutionPanelController::buildConnectionList()
    const {
  QList<QPair<QString, QString>> result;
  if (!engine_) {
    return result;
  }
  const QJsonObject& topo = engine_->topologyDoc();
  const QJsonArray connsArr =
      topo.value(QStringLiteral("connections")).toArray();
  for (const auto& cv : connsArr) {
    const QJsonObject cobj = cv.toObject();
    const QString cid = cobj.value(QStringLiteral("id")).toString();
    if (cid.isEmpty()) {
      continue;
    }
    const QString device = cobj.value(QStringLiteral("device")).toString();
    const QString devicePort =
        cobj.value(QStringLiteral("devicePort")).toString();
    const QString uutPort = cobj.value(QStringLiteral("port")).toString();
    const QString desc = QStringLiteral("%1.%2 ↔ %3")
                             .arg(device, devicePort, uutPort);
    result.append(qMakePair(cid, desc));
  }
  return result;
}

bool ExecutionPanelController::resolveConnection(const QString& connectionId,
                                                 QString* deviceId,
                                                 QString* devicePort,
                                                 QString* deviceType) const {
  if (!engine_ || !deviceId || !devicePort || !deviceType) {
    return false;
  }
  const QJsonObject& topo = engine_->topologyDoc();
  QString deviceName;
  const QJsonArray connsArr =
      topo.value(QStringLiteral("connections")).toArray();
  for (const auto& cv : connsArr) {
    const QJsonObject cobj = cv.toObject();
    if (cobj.value(QStringLiteral("id")).toString() == connectionId) {
      deviceName = cobj.value(QStringLiteral("device")).toString();
      *devicePort = cobj.value(QStringLiteral("devicePort")).toString();
      break;
    }
  }
  if (deviceName.isEmpty()) {
    return false;
  }
  const QJsonArray devicesArr =
      topo.value(QStringLiteral("devices")).toArray();
  for (const auto& dv : devicesArr) {
    const QJsonObject dobj = dv.toObject();
    if (dobj.value(QStringLiteral("name")).toString() == deviceName) {
      *deviceId = dobj.value(QStringLiteral("id")).toString();
      *deviceType = dobj.value(QStringLiteral("deviceType")).toString();
      return !deviceId->isEmpty();
    }
  }
  return false;
}

QString ExecutionPanelController::connectionDescription(
    const QString& connectionId) const {
  if (!engine_) {
    return connectionId;
  }
  const QJsonObject& topo = engine_->topologyDoc();
  const QJsonArray connsArr =
      topo.value(QStringLiteral("connections")).toArray();
  for (const auto& cv : connsArr) {
    const QJsonObject cobj = cv.toObject();
    if (cobj.value(QStringLiteral("id")).toString() == connectionId) {
      return QStringLiteral("%1.%2 ↔ %3")
          .arg(cobj.value(QStringLiteral("device")).toString(),
               cobj.value(QStringLiteral("devicePort")).toString(),
               cobj.value(QStringLiteral("port")).toString());
    }
  }
  return connectionId;
}

// ══════════════════════════════════════════════════════════════════════════════
// syncProjectMonitorsToFile — 当前监听器数组写回 .etproj（决策 5）
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionPanelController::syncProjectMonitorsToFile() {
  auto* mm = monitor_manager_;
  if (!mm) {
    return;
  }
  QJsonArray arr;
  for (const auto& entry : mm->monitorTree()) {
    QJsonObject obj;
    obj[QStringLiteral("connectionId")] = entry.connectionId;
    obj[QStringLiteral("name")] = entry.name;
    obj[QStringLiteral("displayMode")] = entry.displayMode;
    arr.append(obj);
  }
  const bool ok =
      etest::core::project::ProjectManager::instance().setMonitors(arr);
  if (!ok) {
    LOG_WARN("VISUAL", "写回监听器到 .etproj 失败");
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// subscribeVisualizer — 将监听器订阅到某个可视化组件（勾选和拓扑重载重订阅复用）
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionPanelController::subscribeVisualizer(
    const QString& connectionId, SignalVisualizer* vis) {
  if (!vis || !monitor_manager_) {
    return;
  }
  QPointer<SignalVisualizer> visGuard(vis);
  monitor_manager_->subscribe(
      connectionId,
      [visGuard](const etest::engine::MonitorSample& s) {
        if (visGuard) {
          visGuard->onSampleCaptured(s);
        }
      });
}

// ══════════════════════════════════════════════════════════════════════════════
// loadProjectMonitors — 从 .etproj 监听器数组 + 引擎拓扑 JSON 重建监听器（幂等）
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionPanelController::loadProjectMonitors() {
  if (!monitor_manager_ || !engine_) {
    return;
  }
  auto& proj_mgr = etest::core::project::ProjectManager::instance();
  if (!proj_mgr.isProjectOpen()) {
    return;
  }
  const auto* project = proj_mgr.currentProject();
  if (!project) {
    return;
  }
  monitor_manager_->loadMonitors(project->monitors(), engine_->topologyDoc());
}

}  // namespace etest::app
