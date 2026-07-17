#include "ExecutionPanelController.h"

#include <QAction>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QStackedWidget>
#include <QWidget>
#include "ExecutionDashboard.h"
#include "SignalTreePanel.h"
#include "VisualizationArea.h"
#include "engine/MonitorManager.h"
#include "visualizers/SignalVisualizer.h"
#include "visualizers/VisualizerFactory.h"


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
#include "widgets/BottomContainerWidget.h"
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
      new QLabel(QStringLiteral("✅ 0  ❌ 0  ⏱ 0s"), parent_widget_);
}

void ExecutionPanelController::postInit(
    ExecutionOutputPanel* output_panel,
    etest::core::SignalRegistry* signal_registry,
    std::shared_ptr<icd::Repository> icd_repository,
    EditorManager* editor_mgr,
    TestProgramManagerWidget* test_program_mgr,
    ProblemsPanel* problems_panel,
    BottomContainerWidget* bottom_container,
    AppStatusBarController* status_bar_ctrl) {
  output_panel_ = output_panel;
  signal_registry_ = signal_registry;
  icd_repository_ = std::move(icd_repository);
  editor_mgr_ = editor_mgr;
  test_program_mgr_ = test_program_mgr;
  problems_panel_ = problems_panel;
  bottom_container_ = bottom_container;
  status_bar_ctrl_ = status_bar_ctrl;
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
                state_text = QStringLiteral("已完成 (✅%1 ❌%2)")
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

  // 引擎启动后刷新监听器树
  connect(engine_, &etest::engine::TestExecutionEngine::engineStarted, this,
          &ExecutionPanelController::refreshMonitorTree);

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
  if (!engine_) {
    act_run_->setEnabled(true);
    act_run_all_->setEnabled(true);
    act_pause_->setEnabled(false);
    act_stop_->setEnabled(false);
    act_verify_->setEnabled(true);
    return;
  }
  auto state = engine_->state();
  switch (state) {
    case etest::engine::EngineState::Idle:
    case etest::engine::EngineState::Finished:
      act_run_->setEnabled(true);
      act_run_all_->setEnabled(true);
      act_pause_->setEnabled(false);
      act_stop_->setEnabled(false);
      act_verify_->setEnabled(true);
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
      break;
    case etest::engine::EngineState::Error:
      act_run_->setEnabled(true);
      act_run_all_->setEnabled(true);
      act_pause_->setEnabled(false);
      act_stop_->setEnabled(false);
      act_verify_->setEnabled(true);
      break;
  }
}

void ExecutionPanelController::run() {
  LOG_INFO("MAIN_UI", "点击「运行」");

  // 0. 前提检查
  if (debug_widget_ && !debug_widget_->canRun()) {
    QMessageBox::warning(parent_widget_, QStringLiteral("运行"),
                         QStringLiteral("运行前提不满足，请先执行验证"));
    return;
  }

  // 1. 获取测试程序数据
  etest::app::TestProgramData data;
  auto* editor = editor_mgr_ ? editor_mgr_->currentEditor() : nullptr;
  auto* prog_editor =
      editor ? qobject_cast<TestProgramEditorWidget*>(editor->widget())
             : nullptr;
  if (prog_editor) {
    if (editor->isModified()) {
      editor->save();
    }
    data = prog_editor->programData();
  } else if (test_program_mgr_) {
    data = test_program_mgr_->loadSelectedProgramData();
    if (data.name.isEmpty()) {
      QMessageBox::information(
          parent_widget_, QStringLiteral("运行"),
          QStringLiteral("请先打开一个测试程序或从列表中选中一个"));
      return;
    }
  }

  if (data.cases.isEmpty()) {
    QMessageBox::warning(parent_widget_, QStringLiteral("运行"),
                         QStringLiteral("测试程序中没有测试用例，无法运行"));
    return;
  }

  // 2. 创建引擎（registry 和 repository 已在构造时传入）
  createEngine();
  if (!engine_) {
    return;
  }

  // 加载拓扑设备
  {
    auto& proj_mgr = etest::core::project::ProjectManager::instance();
    if (proj_mgr.isProjectOpen()) {
      QString topo_dir =
          proj_mgr.currentProjectRoot() + QStringLiteral("/topology");
      QDir topo_dir_obj(topo_dir);
      if (topo_dir_obj.exists()) {
        const auto topo_files = topo_dir_obj.entryInfoList(
            {QStringLiteral("*.etopo")}, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : topo_files) {
          engine_->loadTopology(fi.absoluteFilePath());
        }
      }
    }
  }

  // 3. 设置程序数据
  current_program_name_ = data.name;
  engine_->setProgram(convertProgram(data));

  // 重置统计
  pass_count_ = 0;
  fail_count_ = 0;
  if (status_bar_ctrl_) {
    status_bar_ctrl_->setExecStats(0, 0, 0);
  }

  // 4. 启动 + 切换到运行态
  engine_->start();
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
  if (!problems_panel_) {
    return;
  }
  auto* problems = problems_panel_;
  problems->clearProblems();
  int errors = 0;
  int warnings = 0;

  // 1. 项目已打开
  auto& proj_mgr = etest::core::project::ProjectManager::instance();
  bool project_open = proj_mgr.isProjectOpen();
  if (!project_open) {
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("错误"),
                         QStringLiteral("未打开项目"));
    errors++;
  }

  // 2. ICD 协议已定义
  bool icd_loaded = icd_repository_ && !icd_repository_->frames().empty();
  if (!icd_loaded) {
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("错误"),
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
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("错误"),
                         QStringLiteral("未找到拓扑文件"));
    errors++;
  }

  // 4. 拓扑已绑定信号
  bool signal_bound =
      signal_registry_ && !signal_registry_->registeredDeviceIds().isEmpty();
  if (!signal_bound) {
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("警告"),
                         QStringLiteral("拓扑未绑定信号"));
    warnings++;
  }

  // 5. 测试程序可用
  bool has_program = false;
  auto* editor = editor_mgr_ ? editor_mgr_->currentEditor() : nullptr;
  auto* prog_editor =
      editor ? qobject_cast<TestProgramEditorWidget*>(editor->widget())
             : nullptr;
  if (prog_editor) {
    has_program = !prog_editor->programData().cases.isEmpty();
  } else if (test_program_mgr_) {
    auto checked = test_program_mgr_->checkedProgramPaths();
    if (checked.size() >= 2) {
      // 串行模式：至少有一个勾选文件有可用用例
      for (const auto& p : checked) {
        auto data = loadTestProgram(p);
        if (!data.cases.isEmpty()) {
          has_program = true;
          break;
        }
      }
    } else {
      // 单次模式：检查当前选中
      auto data = test_program_mgr_->loadSelectedProgramData();
      has_program = !data.cases.isEmpty();
    }
  }
  if (!has_program) {
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("错误"),
                         QStringLiteral("未选择有效的测试程序"));
    errors++;
  }

  // 6. 硬件/Mock 状态
  if (!topo_exists && !signal_bound) {
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("警告"),
                         QStringLiteral("硬件/Mock 未配置"));
    warnings++;
  }

  problems->showSummary(errors, warnings);

  // 有问题时自动切到问题面板
  if ((errors > 0 || warnings > 0) && bottom_container_) {
    int idx = bottom_container_->indexOf(problems_panel_);
    if (idx >= 0) {
      bottom_container_->setCurrentPanel(idx);
      bottom_container_->show();
    }
  }

  // 刷新执行调试面板
  if (debug_widget_) {
    debug_widget_->setDependencies(signal_registry_, icd_repository_.get());
  }
}

void ExecutionPanelController::runAll() {
  LOG_INFO("MAIN_UI", "点击「运行全部」");

  // 如果当前编辑器打开了测试程序，退化为单次运行
  auto* editor = editor_mgr_ ? editor_mgr_->currentEditor() : nullptr;
  auto* prog_editor =
      editor ? qobject_cast<TestProgramEditorWidget*>(editor->widget())
             : nullptr;
  if (prog_editor) {
    run();
    return;
  }

  // 从管理器列表获取勾选的文件
  QStringList paths;
  if (test_program_mgr_) {
    paths = test_program_mgr_->checkedProgramPaths();
  }

  if (paths.size() < 2) {
    // 少于 2 个勾选 → 退化为单次运行
    run();
    return;
  }

  // 多个勾选 → 串行队列执行
  run_queue_ = paths;
  runNextInQueue();
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

  // 加载拓扑设备
  {
    auto& proj_mgr = etest::core::project::ProjectManager::instance();
    if (proj_mgr.isProjectOpen()) {
      QString topo_dir =
          proj_mgr.currentProjectRoot() + QStringLiteral("/topology");
      QDir topo_dir_obj(topo_dir);
      if (topo_dir_obj.exists()) {
        const auto topo_files = topo_dir_obj.entryInfoList(
            {QStringLiteral("*.etopo")}, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : topo_files) {
          engine_->loadTopology(fi.absoluteFilePath());
        }
      }
    }
  }

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
            run_queue_.removeFirst();
            runNextInQueue();
          });

  // 启动
  engine_->start();

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

  // ── SignalTreePanel checkbox → 创建/移除可视化组件 + 订阅/取消订阅 ──
  connect(dashboard_->signalTreePanel(), &SignalTreePanel::checkStateChanged,
          this, [this](int mi, int ci, bool checked) {
            auto* monitorMgr = engine_ ? engine_->monitorManager() : nullptr;
            if (!monitorMgr) {
              return;
            }

            if (checked) {
              // 创建可视化组件（Phase 1：displayMode/signalType 均用默认值）
              auto* vis = createVisualizerFor(
                  mi, ci, QStringLiteral("auto"), QString(),
                  QStringLiteral("Monitor %1 Ch%2").arg(mi).arg(ci), nullptr);
              if (vis) {
                // 先订阅再添加到可视化区（确保回调不会在未订阅时触发）
                monitorMgr->subscribe(
                    mi, ci, [vis](const etest::engine::MonitorSample& sample) {
                      vis->onSampleCaptured(sample);
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
            auto* tree = dashboard_ ? dashboard_->signalTreePanel() : nullptr;
            if (tree) {
              tree->uncheckChannel(mi, ci);
            }
          });

  // ── 如果引擎已就绪，立即加载监听器树 ──
  refreshMonitorTree();
}

void ExecutionPanelController::refreshMonitorTree() {
  if (!dashboard_ || !engine_) {
    return;
  }
  auto* mm = engine_->monitorManager();
  if (!mm) {
    return;
  }
  dashboard_->signalTreePanel()->setMonitorTree(mm->monitorTree());
}

}  // namespace etest::app
