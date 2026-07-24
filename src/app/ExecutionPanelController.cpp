#include "ExecutionPanelController.h"

#include <QAction>

#include <QDateTime>
#include <QDialog>
#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QStackedWidget>
#include <QWidget>
#include "ExecutionDashboard.h"
#include "SignalTreePanel.h"
#include "VisualizationArea.h"
#include "engine/MonitorManager.h"
#include "visualizers/SignalVisualizer.h"
#include "visualizers/VisualizerFactory.h"
#include "visualizers/WaveformWidget.h"


#include "AppIconProvider.h"
#include "AppStatusBarController.h"
#include "EditorManager.h"
#include "ExecutionDebugWidget.h"
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
#include "ProjectInfo.h"
#include "widgets/ProgramSelectionPopup.h"
#include "widgets/ExecutionOutputPanel.h"
#include "widgets/ProblemsPanel.h"


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
  act_clear_data_ = new QAction(QIcon(), QStringLiteral("清空数据"), parent_widget_);
  act_clear_data_->setEnabled(false);
  connect(act_clear_data_, &QAction::triggered, this,
          &ExecutionPanelController::clearData);
  popup_ = new ProgramSelectionPopup(parent_widget_);
  connect(popup_, &ProgramSelectionPopup::selectionChanged, this,
          &ExecutionPanelController::updateRunControls);

  signal_tree_ = new SignalTreePanel(parent_widget_);
  act_select_channels_ =
      new QAction(AppIconProvider::instance().icon(QStringLiteral("signal")),
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
            auto& proj_mgr = etest::core::project::ProjectManager::instance();
            QString report_dir =
                proj_mgr.currentProjectRoot() + QStringLiteral("/reports");
            QDir().mkpath(report_dir);
            QString timestamp =
                QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
            QString etlog_path = report_dir + QStringLiteral("/") +
                                 current_program_name_ + QStringLiteral("_") +
                                 timestamp + QStringLiteral(".etlog");
            engine_->saveReport(etlog_path);
          });
}

void ExecutionPanelController::syncControlStates() {
  // 刷新 popup 程序列表，确保状态检查时列表已填充
  // refreshList() 保留已有选中状态（selected_），不会丢失用户选择
  if (popup_ && etest::core::project::ProjectManager::instance().isProjectOpen()) {
    popup_->refreshList();
  }

  if (!engine_) {
    act_run_->setEnabled(checkCanRun());
    act_run_all_->setEnabled(checkCanRunAll());
    act_pause_->setEnabled(false);
    act_stop_->setEnabled(false);
    act_verify_->setEnabled(checkCanVerify());
    act_clear_data_->setEnabled(false);
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
      break;
    case etest::engine::EngineState::Error:
      act_run_->setEnabled(checkCanRun());
      act_run_all_->setEnabled(checkCanRunAll());
      act_pause_->setEnabled(false);
      act_stop_->setEnabled(false);
      act_verify_->setEnabled(checkCanVerify());
      act_clear_data_->setEnabled(true);
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

// 清空 MonitorManager 的结构（lookup_table_/tree_cache_/subscribers_）与运行时数据（buffer_）。
// 注意：clearStructure 会清空 subscribers_，dashboard 中的可视化组件将收不到后续采样，
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
    return;
  }
  const auto topo_files = topo_dir_obj.entryInfoList(
      {QStringLiteral("*.etopo")}, QDir::Files, QDir::Name);

  auto* mm = monitor_manager_;
  if (!mm) {
    return;
  }

  bool tree_empty = mm->monitorTree().isEmpty();

  if (tree_empty) {
    // ── 首次加载 / 引擎重建后：直接追加 ──
    for (const QFileInfo& fi : topo_files) {
      engine_->loadTopology(fi.absoluteFilePath());
    }
    // 记录 mtime 快照
    topo_mtimes_.clear();
    for (const QFileInfo& fi : topo_files) {
      topo_mtimes_[fi.absoluteFilePath()] = fi.lastModified();
    }
    // 恢复 UI（首次加载时 activeChannels() 为空，退化为全 Unchecked）
    refreshMonitorTree();
    return;
  }

  // ── tree_cache_ 非空：检测拓扑变化 ──

  // mtime 快照为空但树已有数据（setDashboard → refreshMonitorTree 早于首次 syncProjectTopologies）
  // 直接记录快照并跳过，无需重建
  if (topo_mtimes_.isEmpty()) {
    for (const QFileInfo& fi : topo_files) {
      topo_mtimes_[fi.absoluteFilePath()] = fi.lastModified();
    }
    return;
  }

  bool changed = false;
  if (topo_files.size() != topo_mtimes_.size()) {
    changed = true;
  } else {
    for (const QFileInfo& fi : topo_files) {
      auto it = topo_mtimes_.constFind(fi.absoluteFilePath());
      if (it == topo_mtimes_.constEnd() || it.value() != fi.lastModified()) {
        changed = true;
        break;
      }
    }
  }

  if (!changed) {
    // mtime 没变 + 文件列表一致：跳过
    return;
  }

  // ── 拓扑变化：全量重建 ──
  engine_->clearTopologyState();
  clearMonitorState();
  for (const QFileInfo& fi : topo_files) {
    engine_->loadTopology(fi.absoluteFilePath());
  }
  topo_mtimes_.clear();
  for (const QFileInfo& fi : topo_files) {
    topo_mtimes_[fi.absoluteFilePath()] = fi.lastModified();
  }
  refreshMonitorTree();
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

  // 4. 选中 ≥2 个 → 串行队列
  if (paths.size() > 1) {
    run_queue_ = paths;
    runNextInQueue();
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
  run_queue_.clear();  // 清空队列，防 engineFinished 推进下一个程序
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
                         QStringLiteral("错误"),
                         QStringLiteral("未打开项目"));
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

  // 4. 全集串行队列执行
  run_queue_ = all_paths;
  runNextInQueue();
}

bool ExecutionPanelController::checkUnsavedAndPrompt(const QStringList& paths) const {
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
    msg += QStringLiteral("  ") + QFileInfo(f).fileName() + QStringLiteral("\n");
  }
  msg += QStringLiteral("\n请选择操作：");

  QMessageBox box(parent_widget_);
  box.setWindowTitle(QStringLiteral("运行前保存"));
  box.setText(msg);
  auto* saveBtn = box.addButton(QStringLiteral("保存并运行"), QMessageBox::AcceptRole);
  auto* runBtn = box.addButton(QStringLiteral("不保存运行"), QMessageBox::DestructiveRole);
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
            QMessageBox::warning(
                const_cast<QWidget*>(parent_widget_),
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

void ExecutionPanelController::runNextInQueue() {
  if (run_queue_.isEmpty()) {
    return;
  }

  // 加载队列中的下一个程序
  QString path = run_queue_.first();
  etest::app::TestProgramData data = loadTestProgram(path);
  if (data.name.isEmpty() || data.cases.isEmpty()) {
    run_queue_.removeFirst();
    runNextInQueue();  // 跳过无效项
    return;
  }

  LOG_INFO("MAIN_UI", "队列执行测试程序 [name={}]", data.name.toStdString());

  // 重置引擎（每次创建全新的引擎实例）
  if (engine_) {
    destroyEngine();
  }
  createEngine();
  if (!engine_) {
    run_queue_.clear();
    return;
  }

  // 同步拓扑数据（mtime 没变则跳过，引擎重建后 tree_cache_ 为空走首次加载路径）
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

  // 引擎结束后触发下一个
  connect(engine_, &etest::engine::TestExecutionEngine::engineFinished, this,
          [this]() {
            if (run_queue_.isEmpty()) {
              return;  // 队列已空（runAll 完成 / stop 清空），不再推进
            }
            run_queue_.removeFirst();
            runNextInQueue();
          });

  // 启动
  engine_->start();
  // 切到「输出」tab（运行中应看到运行日志）
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

// ══════════════════════════════════════════════════════════════════════════════
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

  // ── SignalTreePanel checkbox → 创建/移除可视化组件 + 订阅/取消订阅 ──
  connect(signal_tree_, &SignalTreePanel::checkStateChanged,
          this, [this](int mi, int ci, bool checked) {
            LOG_INFO("VISUAL",
                     "checkStateChanged mi={} ci={} checked={}",
                     mi, ci, checked);
            auto* monitorMgr = monitor_manager_;
            if (!monitorMgr) {
              return;
            }

            if (checked) {
              // 从拓扑 tap 读取 displayMode，未配置则回退 "auto"
              QString mode = monitorMgr->displayMode(mi, ci);
              if (mode.isEmpty()) {
                mode = QStringLiteral("auto");
              }
              auto* vis = createVisualizerFor(
                  mi, ci, mode, QString(),
                  QStringLiteral("Monitor %1 Ch%2").arg(mi).arg(ci), nullptr);
              if (vis) {
                LOG_INFO("ENGINE", "创建可视化 mi={} ci={} mode={} type={}",
                         mi, ci, mode.toStdString(),
                         vis->metaObject()->className());
                // 波形组件需要先 addTrace，否则 onSampleCaptured 丢弃数据
                if (auto* wave = qobject_cast<WaveformWidget*>(vis)) {
                  // 按通道索引取色，保证同屏多通道可区分
                  static const QColor kColors[] = {
                      QColor(0, 120, 215), QColor(229, 57, 53),
                      QColor(67, 160, 71), QColor(255, 152, 0),
                      QColor(156, 39, 176), QColor(0, 151, 167),
                      QColor(121, 85, 72), QColor(158, 158, 158)};
                  QColor color = kColors[ci % 8];
                  wave->addTrace(mi, ci, color);
                }
                // 先订阅再添加到可视化区（确保回调不会在未订阅时触发）
                // 用 QPointer 保护，防 worker 线程排队事件在 widget 销毁后到达
                QPointer<SignalVisualizer> visGuard(vis);
                monitorMgr->subscribe(
                    mi, ci, [visGuard, mi, ci](const etest::engine::MonitorSample& sample) {
                      if (visGuard) {
                        visGuard->onSampleCaptured(sample);
                      } else {
                        LOG_WARN("VISUAL", "subscriber 回调 visGuard=null, mi={} ci={}", mi, ci);
                      }
                    });
                dashboard_->visualizationArea()->addVisualizer(mi, ci, vis);
              }
            } else {
              // 必须先取消订阅再删除 widget（防 callback 在野指针上触发）
              monitorMgr->unsubscribe(mi, ci);
              dashboard_->visualizationArea()->removeVisualizer(mi, ci);
            }
          });

  // ── 可视化区右键关闭 → 同步取消勾选信号树 ──
  // 取消勾选会触发 checkStateChanged(false) → unsubscribe + removeVisualizer
  connect(dashboard_->visualizationArea(), &VisualizationArea::visualizerClosed,
          this, [this](int mi, int ci) {
            signal_tree_->uncheckChannel(mi, ci);
          });

  // ── 如果引擎已就绪，立即加载监听器树 ──
  refreshMonitorTree();
}

void ExecutionPanelController::refreshMonitorTree() {
  auto* mm = monitor_manager_;
  if (!mm || !signal_tree_) {
    return;
  }
  signal_tree_->setMonitorTree(mm->monitorTree(),
      dashboard_ ? dashboard_->visualizationArea()->activeChannels()
                 : QList<QPair<int, int>>());
}

// ══════════════════════════════════════════════════════════════════════════════
// clearData — 清空所有可视化组件的采样数据（Ribbon 按钮触发）
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionPanelController::clearData() {
  if (!dashboard_) {
    return;
  }
  auto* visArea = dashboard_->visualizationArea();
  QList<QPair<int, int> > channels = visArea->activeChannels();
  for (int i = 0; i < channels.size(); ++i) {
    int mi = channels.at(i).first;
    int ci = channels.at(i).second;
    SignalVisualizer* vis = visArea->visualizer(mi, ci);
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
    // 先清可视化组件（clearAll 发射 visualizerClosed → uncheckChannel 回调需 node_map_ 有效）
    dashboard_->visualizationArea()->clearAll();
    // 再清树
    signal_tree_->clearTree();
  }
  // 清理 MonitorManager 结构与运行时数据（项目切换时避免残留）
  clearMonitorState();
  // 清理 mtime 缓存
  topo_mtimes_.clear();
}

void ExecutionPanelController::showChannelSelectionDialog() {
  if (!signal_tree_dialog_) {
    signal_tree_dialog_ = new QDialog(parent_widget_);
    signal_tree_dialog_->setWindowTitle(QStringLiteral("通道选择"));
    signal_tree_dialog_->resize(380, 480);
    auto* layout = new QVBoxLayout(signal_tree_dialog_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(signal_tree_);
  }
  signal_tree_dialog_->exec();
}

}  // namespace etest::app
