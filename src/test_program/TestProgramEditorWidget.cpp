#include "TestProgramEditorWidget.h"

#include <QAction>
#include <QDockWidget>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSize>
#include <QStandardItemModel>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "ConfigManager.h"
#include "control_flow_config.h"
#include "StepDetailPanel.h"
#include "StepTableWidget.h"
#include "StepValidation.h"
#include "VerticalTabListDelegate.h"
#include "common/AppIconProvider.h"
#include "common/ThemeManager.h"
#include "config/ConfigDefs.h"
#include "libui/dock_title_bar/DockTitleBar.h"
#include "libui/styles/TabBarStyle.h"


namespace etest::app {

namespace {

// kColExtra 是动态含义列，按命令返回应显示的文本：
//   SET/DELAY=延迟(ms), VERIFY=容差min, WAIT/WHILE/IF=条件值, INJECT_FAULT=故障值
QString extraCellText(const TestStepData& step) {
  const QString& cmd = step.cmd;
  if (cmd == QStringLiteral("SET") || cmd == QStringLiteral("DELAY")) {
    return QString::number(step.delayMs);
  }
  if (cmd == QStringLiteral("VERIFY")) {
    return step.tolerance.enabled ? QString::number(step.tolerance.min)
                                  : QString();
  }
  if (cmd == QStringLiteral("WAIT") ||
      (kControlFlowEnabled && (cmd == QStringLiteral("WHILE") ||
                               cmd == QStringLiteral("IF")))) {
    return step.condition.value.toString();
  }
  if (cmd == QStringLiteral("INJECT_FAULT")) {
    return step.fault.value.toString();
  }
  return QString();
}

// kColExtra2 动态列：VERIFY=容差max, WAIT/WHILE=间隔(ms)
QString extra2CellText(const TestStepData& step) {
  const QString& cmd = step.cmd;
  if (cmd == QStringLiteral("VERIFY")) {
    return step.tolerance.enabled ? QString::number(step.tolerance.max)
                                  : QString();
  }
  if (cmd == QStringLiteral("WAIT") ||
      (kControlFlowEnabled && cmd == QStringLiteral("WHILE"))) {
    return QString::number(step.loopIntervalMs);
  }
  return QString();
}

}  // namespace

TestProgramEditorWidget::TestProgramEditorWidget(const QString& filePath,
                                                 QWidget* parent)
    : QMainWindow(parent) {
  initUi();
  initSignals();

  if (!filePath.isEmpty() && !filePath.startsWith("editor://") &&
      QFileInfo::exists(filePath)) {
    current_file_ = filePath;
    loadFile(filePath);
  } else {
    current_file_ = filePath.startsWith("editor://") ? QString() : filePath;
    newProgram();
  }
}

void TestProgramEditorWidget::setEmbeddedMode(bool embedded) {
  embedded_ = embedded;
  if (embedded_) {
    menuBar()->hide();
  } else {
    menuBar()->show();
  }
}

void TestProgramEditorWidget::newProgram() {
  QString oldId = editorId();
  current_file_.clear();

  TestProgramData suite;
  suite.name = QStringLiteral("未命名测试程序");

  loading_ = true;
  loadProgramToUi(suite);
  loading_ = false;

  resetSnapshots(true);
  setModified(false);
  updateActions();

  if (oldId != editorId()) {
    emit editorIdChanged(oldId, editorId());
  }
}

// ── M0: ISignalSelection 注入 ──

void TestProgramEditorWidget::setSignalSelection(ISignalSelection* sel) {
  signal_selection_ = sel;
  setup_table_->setSignalSelection(sel);
  teardown_table_->setSignalSelection(sel);
  for (int t = 2; t < tab_widget_->count(); ++t) {
    if (auto* table = qobject_cast<StepTableWidget*>(tab_widget_->widget(t)))
      table->setSignalSelection(sel);
  }
}

void TestProgramEditorWidget::setRegistry(etest::core::SignalRegistry* reg) {
  registry_ = reg;
  setup_table_->setRegistry(reg);
  teardown_table_->setRegistry(reg);
  for (int t = 2; t < tab_widget_->count(); ++t) {
    if (auto* table = qobject_cast<StepTableWidget*>(tab_widget_->widget(t)))
      table->setRegistry(reg);
  }
}

void TestProgramEditorWidget::initUi() {
  setAutoFillBackground(true);

  // ── Icon loader (theme-aware) ──
  auto tpIcon = [](const QString& name) {
    return etest::app::AppIconProvider::instance().icon(name);
  };

  // ── QToolBar ──
  auto* toolbar = addToolBar(QStringLiteral("测试程序工具栏"));
  toolbar->setObjectName(QStringLiteral("testProgramToolbar"));
  toolbar->setMovable(false);
  toolbar->setFloatable(false);

  // ── 撤销 / 重做 ──
  undo_action_ =
      new QAction(tpIcon(QStringLiteral("undo")), QStringLiteral("撤销"), this);
  undo_action_->setShortcut(QKeySequence::Undo);
  undo_action_->setToolTip(QStringLiteral("撤销 (Ctrl+Z)"));
  undo_action_->setEnabled(false);
  toolbar->addAction(undo_action_);

  redo_action_ =
      new QAction(tpIcon(QStringLiteral("redo")), QStringLiteral("重做"), this);
  redo_action_->setShortcut(QKeySequence::Redo);
  redo_action_->setToolTip(QStringLiteral("重做 (Ctrl+Y)"));
  redo_action_->setEnabled(false);
  toolbar->addAction(redo_action_);

  toolbar->addSeparator();

  add_case_action_ = new QAction(tpIcon(QStringLiteral("testprog_add_case")),
                                 QStringLiteral("添加用例"), this);
  add_case_action_->setToolTip(QStringLiteral("添加测试用例"));
  toolbar->addAction(add_case_action_);

  remove_case_action_ =
      new QAction(tpIcon(QStringLiteral("testprog_remove_case")),
                  QStringLiteral("删除用例"), this);
  remove_case_action_->setToolTip(QStringLiteral("删除当前测试用例"));
  toolbar->addAction(remove_case_action_);

  toolbar->addSeparator();

  add_step_action_ = new QAction(tpIcon(QStringLiteral("testprog_add_step")),
                                 QStringLiteral("添加步骤"), this);
  add_step_action_->setToolTip(QStringLiteral("添加测试步骤"));
  toolbar->addAction(add_step_action_);

  remove_step_action_ =
      new QAction(tpIcon(QStringLiteral("testprog_remove_step")),
                  QStringLiteral("删除步骤"), this);
  remove_step_action_->setToolTip(QStringLiteral("删除当前测试步骤"));
  toolbar->addAction(remove_step_action_);

  move_up_action_ = new QAction(tpIcon(QStringLiteral("testprog_move_up")),
                                QStringLiteral("上移"), this);
  move_up_action_->setToolTip(QStringLiteral("上移步骤"));
  toolbar->addAction(move_up_action_);

  move_down_action_ = new QAction(tpIcon(QStringLiteral("testprog_move_down")),
                                  QStringLiteral("下移"), this);
  move_down_action_->setToolTip(QStringLiteral("下移步骤"));
  toolbar->addAction(move_down_action_);

  auto* spacer = new QWidget(toolbar);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolbar->addWidget(spacer);

  toolbar->addSeparator();

  // 切换标签栏方向（横向 QTabBar / 纵向 QListView）
  toggle_orientation_action_ =
      new QAction(tpIcon(QStringLiteral("testprog_tab_vertical")),
                  QStringLiteral("纵向标签"), this);
  toggle_orientation_action_->setToolTip(QStringLiteral("切换纵向/横向标签栏"));
  toggle_orientation_action_->setCheckable(true);
  toolbar->addAction(toggle_orientation_action_);

  auto* content = new QWidget(this);
  auto* main_layout = new QVBoxLayout(content);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  // ── Central：步骤表格 ──
  tab_widget_ = new QTabWidget(content);
  tab_widget_->tabBar()->installEventFilter(this);
  tab_widget_->tabBar()->setElideMode(Qt::ElideRight);
  tab_widget_->tabBar()->setUsesScrollButtons(true);
  // Chrome 风格 tab 形状（参考 draw_tab_shape demo）
  TabBarStyle::install(tab_widget_->tabBar());
  tab_widget_->tabBar()->setMovable(false);
  tab_widget_->setIconSize(QSize(16, 16));

  setup_table_ = new StepTableWidget(CommandTypeDelegate::Full, this);
  tab_widget_->addTab(setup_table_, tpIcon(QStringLiteral("testprog_tab_init")),
                      QStringLiteral("初始化"));

  teardown_table_ = new StepTableWidget(CommandTypeDelegate::Full, this);
  tab_widget_->addTab(teardown_table_,
                      tpIcon(QStringLiteral("testprog_tab_cleanup")),
                      QStringLiteral("清理"));

  main_layout->addWidget(tab_widget_, 1);

  // ── 校验状态栏 ──
  validation_label_ = new QLabel(content);
  validation_label_->setObjectName(
      QStringLiteral("testProgramValidationLabel"));
  validation_label_->setVisible(false);
  main_layout->addWidget(validation_label_);

  setCentralWidget(content);

  // ── Info Dock：套件名称 + 描述 ──
  auto* info_widget = new QWidget(this);
  auto* info_layout = new QVBoxLayout(info_widget);
  info_layout->setContentsMargins(8, 8, 8, 8);
  info_layout->setSpacing(6);

  auto* name_label = new QLabel(QStringLiteral("套件名称:"), info_widget);
  suite_name_edit_ = new QLineEdit(info_widget);
  info_layout->addWidget(name_label);
  info_layout->addWidget(suite_name_edit_);

  auto* desc_label = new QLabel(QStringLiteral("描述:"), info_widget);
  suite_desc_edit_ = new QTextEdit(info_widget);
  suite_desc_edit_->setPlaceholderText(QStringLiteral("套件描述..."));
  suite_desc_edit_->setMaximumHeight(80);
  info_layout->addWidget(desc_label);
  info_layout->addWidget(suite_desc_edit_);
  info_layout->addStretch();

  info_dock_ = new QDockWidget(QStringLiteral("套件信息"), this);
  info_dock_->setObjectName(QStringLiteral("testProgramInfoDock"));
  info_dock_->setWidget(info_widget);
  info_dock_->setFeatures(QDockWidget::AllDockWidgetFeatures);
  info_dock_->setTitleBarWidget(
      new ::etest::ui::DockTitleBar(QStringLiteral("套件信息"), info_dock_));
  addDockWidget(Qt::RightDockWidgetArea, info_dock_);

  // ── Detail Dock：步骤详情（常驻显示） ──
  step_detail_panel_ = new StepDetailPanel(this);
  detail_dock_ = new QDockWidget(QStringLiteral("步骤详情"), this);
  detail_dock_->setObjectName(QStringLiteral("testProgramDetailDock"));
  detail_dock_->setWidget(step_detail_panel_);
  detail_dock_->setFeatures(QDockWidget::AllDockWidgetFeatures);
  detail_dock_->setTitleBarWidget(
      new ::etest::ui::DockTitleBar(QStringLiteral("步骤详情"), detail_dock_));
  addDockWidget(Qt::RightDockWidgetArea, detail_dock_);

  // ── Vertical Tabs Dock：Edge 风格垂直标签栏 ──
  vertical_tabs_view_ = new QListView(this);
  vertical_tabs_view_->setFrameShape(QFrame::NoFrame);
  vertical_tabs_view_->setMouseTracking(true);
  vertical_tabs_view_->setSelectionMode(QAbstractItemView::SingleSelection);
  vertical_tabs_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  vertical_tabs_view_->setUniformItemSizes(true);
  vertical_tabs_view_->setIconSize(QSize(16, 16));
  vertical_tabs_model_ = new QStandardItemModel(this);
  vertical_tabs_view_->setModel(vertical_tabs_model_);
  vertical_tabs_delegate_ = new VerticalTabListDelegate(this);
  vertical_tabs_view_->setItemDelegate(vertical_tabs_delegate_);

  vertical_tabs_dock_ = new QDockWidget(QStringLiteral("标签页"), this);
  vertical_tabs_dock_->setObjectName(
      QStringLiteral("testProgramVerticalTabsDock"));
  vertical_tabs_dock_->setWidget(vertical_tabs_view_);
  vertical_tabs_dock_->setAllowedAreas(Qt::LeftDockWidgetArea);
  vertical_tabs_dock_->setFeatures(QDockWidget::DockWidgetClosable);
  auto* vt_title_bar = new ::etest::ui::DockTitleBar(QStringLiteral("标签页"),
                                                     vertical_tabs_dock_);
  // 不允许浮动：藏掉标题栏的浮动按钮
  if (auto* float_btn = vt_title_bar->findChild<QToolButton*>(
          QStringLiteral("dockFloatButton"))) {
    float_btn->setVisible(false);
  }
  vertical_tabs_dock_->setTitleBarWidget(vt_title_bar);
  addDockWidget(Qt::LeftDockWidgetArea, vertical_tabs_dock_);

  rebuildVerticalTabs();

  // 初始方向（默认水平）
  const QString orient =
      etest::core::config::ConfigManager::instance().get<QString>(
          etest::core::config::CONFIG_TEST_PROGRAM_TAB_ORIENTATION,
          QStringLiteral("horizontal"));
  applyTabOrientation(orient == QLatin1String("vertical"));
}

void TestProgramEditorWidget::initSignals() {
  connect(suite_name_edit_, &QLineEdit::textChanged, this,
          &TestProgramEditorWidget::onDataChanged);
  connect(suite_desc_edit_, &QTextEdit::textChanged, this,
          &TestProgramEditorWidget::onDataChanged);

  // StepTableWidget 信号
  connectTable(setup_table_);
  connectTable(teardown_table_);

  connect(tab_widget_, &QTabWidget::currentChanged, this, [this](int idx) {
    // 同步纵向标签列表选中
    if (!syncing_vertical_tabs_ && vertical_tabs_model_) {
      syncing_vertical_tabs_ = true;
      if (idx >= 0 && idx < vertical_tabs_model_->rowCount()) {
        vertical_tabs_view_->setCurrentIndex(
            vertical_tabs_model_->index(idx, 0));
      }
      syncing_vertical_tabs_ = false;
    }
    updateActions();
    validateCurrentTable();
    // 切 tab 后刷新面板：面板不会因 tab 切换自动反映新 tab 的选区，
    // 不刷新会导致编辑面板数据写错到新 tab 的选中行
    onStepSelectionChanged();
  });
  connect(add_case_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::onAddCase);
  connect(remove_case_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::onRemoveCase);
  connect(add_step_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::onAddStep);
  connect(remove_step_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::onRemoveStep);
  connect(move_up_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::onMoveUp);
  connect(move_down_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::onMoveDown);
  connect(undo_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::undo);
  connect(redo_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::redo);

  // 详情面板数据变更 → 同步扩展字段到当前行的 UserRole
  // 面板是扩展字段（容差/故障/条件/循环参数/子步骤）的唯一编辑入口，需全量回写；
  // 可见列（cmd/desc/target/value/delay/timeout）由 readStepData 保存时从单元格覆盖。
  connect(step_detail_panel_, &StepDetailPanel::dataChanged, this, [this]() {
    if (loading_ || undo_redo_in_progress_ || validating_) {
      return;
    }
    auto* table = qobject_cast<StepTableWidget*>(tab_widget_->currentWidget());
    int row = table ? table->currentRow() : -1;
    if (table && row >= 0) {
      TestStepData panelStep = step_detail_panel_->stepData();
      TestStepData existing = table->stepExtData(row);
      existing.tolerance = panelStep.tolerance;
      existing.fault = panelStep.fault;
      existing.condition = panelStep.condition;
      existing.loopCount = panelStep.loopCount;
      existing.loopIntervalMs = panelStep.loopIntervalMs;
      existing.subSteps = panelStep.subSteps;
      existing.elseSubSteps = panelStep.elseSubSteps;
      table->setStepExtData(row, existing);
      // 同步动态列显示（容差/条件/故障/间隔）及 description（ActionLog 页编辑内容），
      // 阻塞信号避免重复入快照
      table->model()->blockSignals(true);
      table->setCellText(row, StepTableWidget::kColDesc,
                         panelStep.description);
      table->setCellText(row, StepTableWidget::kColExtra,
                         extraCellText(existing));
      table->setCellText(row, StepTableWidget::kColExtra2,
                         extra2CellText(existing));
      table->model()->blockSignals(false);
    }
    saveSnapshot();
    setModified(true);
    updateActions();
  });

  // 主题切换 → 重新加载 toolbar 图标
  connect(&etest::app::ThemeManager::instance(),
          &etest::app::ThemeManager::themeChanged, this,
          &TestProgramEditorWidget::reloadToolbarIcons);

  // 主题切换 → 刷新表格行高（按新字体动态算，避免 editor 字体被截断）
  connect(&etest::app::ThemeManager::instance(),
          &etest::app::ThemeManager::themeChanged, this, [this]() {
            setup_table_->refreshRowHeight();
            teardown_table_->refreshRowHeight();
            for (int t = 2; t < tab_widget_->count(); ++t) {
              auto* table =
                  qobject_cast<StepTableWidget*>(tab_widget_->widget(t));
              if (table) {
                table->refreshRowHeight();
              }
            }
          });

  // ── 纵向标签栏 ──
  // 切换方向
  connect(toggle_orientation_action_, &QAction::toggled, this,
          [this](bool vertical) { applyTabOrientation(vertical); });
  // dock 关闭按钮 → 切回水平（程序内 setVisible 触发的由 applying_orientation_
  // 拦截）
  connect(vertical_tabs_dock_, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (!applying_orientation_) {
              applyTabOrientation(visible);
            }
          });

  // list → tab 同步
  connect(vertical_tabs_view_->selectionModel(),
          &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex& cur) {
            if (syncing_vertical_tabs_ || !cur.isValid()) {
              return;
            }
            int idx = cur.data(VerticalTabRole::TabIndexRole).toInt();
            if (idx >= 0 && idx < tab_widget_->count()) {
              syncing_vertical_tabs_ = true;
              tab_widget_->setCurrentIndex(idx);
              syncing_vertical_tabs_ = false;
            }
          });

  // 双击重命名（仅用例）
  connect(vertical_tabs_view_, &QListView::doubleClicked, this,
          [this](const QModelIndex& cur) {
            int idx = cur.data(VerticalTabRole::TabIndexRole).toInt();
            if (idx >= 2) {
              renameCase(idx);
            }
          });

  // 右键菜单
  connect(vertical_tabs_view_, &QListView::customContextMenuRequested, this,
          [this](const QPoint& pos) {
            QModelIndex cur = vertical_tabs_view_->indexAt(pos);
            if (!cur.isValid()) {
              return;
            }
            int idx = cur.data(VerticalTabRole::TabIndexRole).toInt();
            QMenu menu(vertical_tabs_view_);
            menu.setObjectName(QStringLiteral("testProgramVerticalTabsMenu"));
            if (idx >= 2) {
              auto* renameAction = menu.addAction(QStringLiteral("重命名"));
              auto* closeAction = menu.addAction(QStringLiteral("关闭用例"));
              QAction* chosen =
                  menu.exec(vertical_tabs_view_->viewport()->mapToGlobal(pos));
              if (chosen == renameAction) {
                renameCase(idx);
              } else if (chosen == closeAction) {
                removeCaseAt(idx, false);
              }
            }
          });

  // delegate 关闭按钮
  connect(vertical_tabs_delegate_, &VerticalTabListDelegate::closeRequested,
          this, [this](int idx) { removeCaseAt(idx, false); });
}

void TestProgramEditorWidget::connectTable(StepTableWidget* table) {
  connect(table, &StepTableWidget::cellDataChanged, this,
          &TestProgramEditorWidget::onDataChanged);
  connect(table, &StepTableWidget::stepSelectionChanged, this,
          &TestProgramEditorWidget::onStepSelectionChanged);
  connect(table, &StepTableWidget::stepSelectionChanged, this,
          &TestProgramEditorWidget::updateActions);
  // 命令列变更 → 动态列头调整
  // 用 QPointer 防止表格删除后 lambda 仍持有悬空指针
  auto* tablePtr = table;
  QPointer<StepTableWidget> weakTable(tablePtr);
  // 命令列变更 → 按新命令重新填充 参数1/参数2 列显示（列头固定，仅刷新值）
  connect(table, &StepTableWidget::cellDataChanged, this,
          [this, weakTable](int row, int col) {
            if (!weakTable) {
              return;  // 表格已删除
            }
            if (col == StepTableWidget::kColCmd) {
              TestStepData step = weakTable->stepExtData(row);
              step.cmd = weakTable->cellText(row, col).trimmed().toUpper();
              // 保留 kColExtra 单元格的 delayMs（SET/DELAY），
              // stepExtData 不存 delayMs 返回 0，会覆盖原值
              if (step.cmd == QStringLiteral("SET") ||
                  step.cmd == QStringLiteral("DELAY")) {
                step.delayMs =
                    weakTable->cellText(row, StepTableWidget::kColExtra)
                        .toInt(nullptr, 10);
              }
              weakTable->model()->blockSignals(true);
              weakTable->setCellText(row, StepTableWidget::kColExtra,
                                     extraCellText(step));
              weakTable->setCellText(row, StepTableWidget::kColExtra2,
                                     extra2CellText(step));
              weakTable->model()->blockSignals(false);
              // 改的是当前选中行 → 刷新面板页面（如 SET→LOOP 切到 Loop 页），
              // 否则面板仍显示旧命令页，编辑会写无关字段进 ext data
              if (row == weakTable->currentRow()) {
                step_detail_panel_->setStepData(
                    readStepData(weakTable.data(), row), false);
              }
            }
          });
}

TestStepData TestProgramEditorWidget::readStepData(StepTableWidget* table,
                                                   int row) const {
  TestStepData step;
  // 优先读扩展数据（含子步骤/条件/容差等）
  step = table->stepExtData(row);
  // 用当前单元格值覆盖可见列（cmd/desc/target/value）
  step.cmd = table->cellText(row, StepTableWidget::kColCmd).trimmed().toUpper();
  step.description = table->cellText(row, StepTableWidget::kColDesc);
  step.target = table->cellText(row, StepTableWidget::kColTarget);
  step.value = table->cellText(row, StepTableWidget::kColValue);
  // kColExtra 动态列：仅 SET/DELAY 的"延迟"从单元格读（cell 是该字段编辑入口）；
  // VERIFY/WAIT/WHILE/IF/INJECT_FAULT 的 kColExtra(kColExtra2) 是纯显示
  // （容差/条件/故障等扩展字段以 ext data 为准），不在此覆盖，避免污染 delayMs
  if (step.cmd == QStringLiteral("SET") || step.cmd == QStringLiteral("DELAY")) {
    step.delayMs =
        table->cellText(row, StepTableWidget::kColExtra).toInt(nullptr, 10);
  }
  step.timeoutMs =
      table->cellText(row, StepTableWidget::kColTimeout).toInt(nullptr, 10);
  return step;
}

// ── 数据变更 ──

void TestProgramEditorWidget::onDataChanged() {
  if (loading_ || undo_redo_in_progress_ || validating_) {
    return;
  }
  saveSnapshot();
  setModified(true);
  validateCurrentTable();
  updateActions();
}

void TestProgramEditorWidget::onAddCase() {
  if (loading_) {
    return;
  }

  auto* table = new StepTableWidget(CommandTypeDelegate::Full, this);
  connectTable(table);
  // M0: 传播信号选择器和 registry
  if (signal_selection_) table->setSignalSelection(signal_selection_);
  if (registry_) table->setRegistry(registry_);
  // 命名去重：找最小不重复编号，避免 undo/redo 按名恢复落到错误 tab
  int n = 1;
  while (true) {
    QString name = QStringLiteral("测试用例 %1").arg(n);
    bool dup = false;
    for (int t = 2; t < tab_widget_->count(); ++t) {
      if (tab_widget_->tabText(t) == name) {
        dup = true;
        break;
      }
    }
    if (!dup) {
      tab_widget_->addTab(
          table,
          etest::app::AppIconProvider::instance().icon(
              QStringLiteral("testprog_tab_case")),
          name);
      break;
    }
    ++n;
  }
  tab_widget_->setCurrentWidget(table);

  rebuildVerticalTabs();
  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onRemoveCase() {
  removeCaseAt(tab_widget_->currentIndex(), true);
}

void TestProgramEditorWidget::onAddStep() {
  if (loading_) {
    return;
  }

  auto* table = qobject_cast<StepTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    return;
  }

  int row = table->rowCount();
  table->setRowCount(row + 1);

  // 抑制 7 次 cellDataChanged 信号，避免污染 undo 栈
  table->model()->blockSignals(true);
  table->setCellText(row, StepTableWidget::kColDesc, QString());
  table->setCellText(row, StepTableWidget::kColCmd, QStringLiteral("SET"));
  table->setCellText(row, StepTableWidget::kColTarget, QString());
  table->setCellText(row, StepTableWidget::kColValue, QString());
  table->setCellText(row, StepTableWidget::kColExtra, QStringLiteral("0"));
  table->setCellText(row, StepTableWidget::kColExtra2, QString());
  table->setCellText(row, StepTableWidget::kColTimeout, QStringLiteral("5000"));
  table->model()->blockSignals(false);

  table->selectRow(row);

  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onRemoveStep() {
  auto* table = qobject_cast<StepTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    return;
  }

  int row = table->currentRow();
  if (row < 0) {
    return;
  }

  table->removeRow(row);
  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onMoveUp() {
  auto* table = qobject_cast<StepTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    return;
  }

  int row = table->currentRow();
  if (row <= 0) {
    return;
  }

  // 通过 QStandardItemModel 交换行
  auto* model = qobject_cast<QStandardItemModel*>(table->model());
  if (!model) {
    return;
  }
  QList<QStandardItem*> items = model->takeRow(row);
  model->insertRow(row - 1, items);
  // 不用显式调 renumberSteps：rowsRemoved/rowsInserted 已在 setRowCount
  // 批量模式外 自动触发 renumberSteps（见 StepTableWidget 构造函数连接）
  table->selectRow(row - 1);

  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onMoveDown() {
  auto* table = qobject_cast<StepTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    return;
  }

  int row = table->currentRow();
  if (row < 0 || row >= table->rowCount() - 1) {
    return;
  }

  auto* model = qobject_cast<QStandardItemModel*>(table->model());
  if (!model) {
    return;
  }
  QList<QStandardItem*> items = model->takeRow(row);
  model->insertRow(row + 1, items);
  // 不用显式调 renumberSteps（同 onMoveUp）
  table->selectRow(row + 1);

  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onStepSelectionChanged() {
  auto* table = qobject_cast<StepTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    step_detail_panel_->clear();
    return;
  }

  int row = table->currentRow();
  if (row < 0) {
    step_detail_panel_->clear();
    return;
  }

  // 始终填充面板，由面板内部根据命令类型切换页面
  TestStepData step = readStepData(table, row);
  step_detail_panel_->setStepData(step, false);
}

// ── 校验 ──

void TestProgramEditorWidget::validateCurrentTable() {
  // RAII 守护：setCellData 触发 signal chain 递归时早返；析构时复位
  ValidateGuard guard(this);
  if (!guard.shouldRun()) {
    return;
  }
  auto* table = qobject_cast<StepTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    updateValidationLabel();
    return;
  }

  int errors = 0;
  int warnings = 0;
  QStringList details;

  for (int i = 0; i < table->rowCount(); ++i) {
    TestStepData step = readStepData(table, i);
    QStringList issues = StepValidation::validateStep(step);
    if (!issues.isEmpty()) {
      // 行标红
      for (int c = 0; c < StepTableWidget::kColCount; ++c) {
        if (table->cellData(i, c, Qt::DisplayRole).isValid()) {
          table->setCellData(i, c, QColor(80, 30, 30), Qt::BackgroundRole);
          table->setCellData(i, c, issues.join(QStringLiteral("\n")),
                             Qt::ToolTipRole);
        }
      }
      for (const QString& issue : issues) {
        if (issue.contains(QStringLiteral("错误")) ||
            issue.contains(QStringLiteral("必须")) ||
            issue.contains(QStringLiteral("不能为空"))) {
          errors++;
        } else {
          warnings++;
        }
        details.append(QStringLiteral("[行%1] %2").arg(i + 1).arg(issue));
      }
    } else {
      for (int c = 0; c < StepTableWidget::kColCount; ++c) {
        table->setCellData(i, c, QVariant(), Qt::BackgroundRole);
        table->setCellData(i, c, QVariant(), Qt::ToolTipRole);
      }
    }
  }

  if (errors > 0 || warnings > 0) {
    QString labelText;
    if (errors > 0) {
      labelText = QStringLiteral("⚠ %1 个错误").arg(errors);
    }
    if (warnings > 0) {
      if (!labelText.isEmpty()) {
        labelText += QStringLiteral(", ");
      }
      labelText += QStringLiteral("%1 个警告").arg(warnings);
    }
    if (!details.isEmpty()) {
      labelText += QStringLiteral(" · %1").arg(details.first());
    }
    validation_label_->setText(labelText);
    validation_label_->setToolTip(details.join(QStringLiteral("\n")));
    validation_label_->setVisible(true);
  } else {
    validation_label_->setVisible(false);
  }
  // validating_ 由 ValidateGuard 析构时复位
}

void TestProgramEditorWidget::updateValidationLabel() {
  validation_label_->setVisible(false);
}

// ── 事件过滤（Tab 双击重命名） ──

bool TestProgramEditorWidget::eventFilter(QObject* obj, QEvent* event) {
  if (obj == tab_widget_->tabBar() &&
      event->type() == QEvent::MouseButtonDblClick) {
    auto* me = static_cast<QMouseEvent*>(event);
    int tabIdx = tab_widget_->tabBar()->tabAt(me->pos());
    if (tabIdx >= 2) {
      tab_widget_->tabBar()->setCurrentIndex(tabIdx);
      renameCase(tabIdx);
    }
    // 不管是不是 case tab、是否成功重命名，都吃掉事件，避免冒泡触发菜单快捷键等
    return true;
  }
  return QMainWindow::eventFilter(obj, event);
}

// ── 快照式撤销/重做 ──

void TestProgramEditorWidget::saveSnapshot() {
  if (snapshot_index_ < snapshots_.size() - 1) {
    snapshots_.resize(snapshot_index_ + 1);
  }
  snapshots_.append(uiToProgram());
  snapshot_index_ = snapshots_.size() - 1;
  while (snapshots_.size() > kMaxSnapshots) {
    snapshots_.removeFirst();
    snapshot_index_--;
    if (clean_snapshot_index_ > 0) {
      clean_snapshot_index_--;
    } else if (clean_snapshot_index_ == 0) {
      clean_snapshot_index_ = -1;
    }
  }
  updateActions();
}

void TestProgramEditorWidget::restoreState(const TestProgramData& state) {
  // 记录选区，undo/redo 后恢复；否则 loadProgramToUi 清选区会导致面板
  // （含子步骤表）不刷新，看起来像撤销没生效
  int prevTab = tab_widget_->currentIndex();
  int prevRow = -1;
  QString prevCaseName;
  if (auto* t = qobject_cast<StepTableWidget*>(tab_widget_->currentWidget())) {
    prevRow = t->currentRow();
  }
  if (prevTab >= 2) {
    prevCaseName = tab_widget_->tabText(prevTab);
  }

  loading_ = true;
  loadProgramToUi(state);
  loading_ = false;

  // 恢复选区：case tab 按 name 找（loadProgramToUi 重建 case tab，index 可能变）
  int restoreTab = prevTab;
  if (prevTab >= 2 && !prevCaseName.isEmpty()) {
    restoreTab = -1;
    for (int t = 2; t < tab_widget_->count(); ++t) {
      if (tab_widget_->tabText(t) == prevCaseName) {
        restoreTab = t;
        break;
      }
    }
  }
  if (restoreTab >= 0 && restoreTab < tab_widget_->count()) {
    tab_widget_->setCurrentIndex(restoreTab);
    if (auto* t =
            qobject_cast<StepTableWidget*>(tab_widget_->widget(restoreTab))) {
      if (prevRow >= 0 && prevRow < t->rowCount()) {
        t->selectRow(prevRow);  // 触发 onStepSelectionChanged → 面板刷新
      }
    }
  }

  if (snapshot_index_ == clean_snapshot_index_) {
    modified_ = false;
    emit modificationChanged(false);
  } else {
    modified_ = true;
    emit modificationChanged(true);
  }
  updateActions();
}

bool TestProgramEditorWidget::canUndo() const {
  return snapshot_index_ > 0;
}

bool TestProgramEditorWidget::canRedo() const {
  return snapshot_index_ < snapshots_.size() - 1;
}

void TestProgramEditorWidget::undo() {
  if (!canUndo()) {
    return;
  }

  undo_redo_in_progress_ = true;
  snapshot_index_--;
  restoreState(snapshots_[snapshot_index_]);
  undo_redo_in_progress_ = false;
  updateActions();
}

void TestProgramEditorWidget::redo() {
  if (!canRedo()) {
    return;
  }

  undo_redo_in_progress_ = true;
  snapshot_index_++;
  restoreState(snapshots_[snapshot_index_]);
  undo_redo_in_progress_ = false;
  updateActions();
}

// ── IEditor 接口 ──

QString TestProgramEditorWidget::displayName() const {
  if (current_file_.isEmpty()) {
    QString name = suite_name_edit_ ? suite_name_edit_->text() : QString();
    return name.isEmpty() ? QStringLiteral("未命名测试程序") : name;
  }
  return QFileInfo(current_file_).fileName();
}

bool TestProgramEditorWidget::isModified() const {
  return modified_;
}

bool TestProgramEditorWidget::save() {
  if (current_file_.isEmpty()) {
    return false;
  }
  return saveFile(current_file_);
}

bool TestProgramEditorWidget::saveAs(const QString& path) {
  if (path.isEmpty()) {
    return false;
  }
  QString oldId = editorId();
  current_file_ = path;
  if (saveFile(path)) {
    emit editorIdChanged(oldId, editorId());
    return true;
  }
  current_file_ = oldId.startsWith("editor://") ? QString() : oldId;
  return false;
}

QString TestProgramEditorWidget::filePath() const {
  return current_file_;
}

QString TestProgramEditorWidget::editorId() const {
  if (current_file_.isEmpty()) {
    return QStringLiteral("editor://testprogram/new");
  }
  return current_file_;
}

QWidget* TestProgramEditorWidget::widget() {
  return this;
}

QString TestProgramEditorWidget::editorType() const {
  return QStringLiteral("testprogram");
}

QObject* TestProgramEditorWidget::signalObject() {
  return this;
}

void TestProgramEditorWidget::openFile(const QString& filePath) {
  QString oldId = editorId();
  current_file_ = filePath.startsWith("editor://") ? QString() : filePath;
  if (oldId != editorId()) {
    emit editorIdChanged(oldId, editorId());
  }

  if (!filePath.isEmpty() && !filePath.startsWith("editor://") &&
      QFileInfo::exists(filePath)) {
    loadFile(filePath);
  }
}

// ── 文件 I/O ──

bool TestProgramEditorWidget::loadFile(const QString& path) {
  TestProgramData suite = loadTestProgram(path);
  if (suite.name.isEmpty() && suite.cases.isEmpty()) {
    return false;
  }

  loading_ = true;
  loadProgramToUi(suite);
  loading_ = false;

  resetSnapshots(true);
  setModified(false);
  updateActions();
  return true;
}

bool TestProgramEditorWidget::saveFile(const QString& path) {
  TestProgramData suite = uiToProgram();
  if (!saveTestProgram(path, suite)) {
    return false;
  }

  clean_snapshot_index_ = snapshot_index_;

  modified_ = false;
  emit modificationChanged(false);
  updateActions();
  return true;
}

void TestProgramEditorWidget::loadProgramToUi(const TestProgramData& suite) {
  suite_name_edit_->setText(suite.name);
  suite_desc_edit_->setText(suite.description);

  // ── 填充表格（Setup / Teardown / Cases） ──
  auto fillTable = [](StepTableWidget* table,
                      const QVector<TestStepData>& steps) {
    table->setRowCount(steps.size());
    for (int i = 0; i < steps.size(); ++i) {
      const auto& step = steps[i];
      table->setCellText(i, StepTableWidget::kColDesc, step.description);
      table->setCellText(i, StepTableWidget::kColCmd, step.cmd);
      table->setCellText(i, StepTableWidget::kColTarget, step.target);
      table->setCellText(i, StepTableWidget::kColValue, step.value.toString());
      table->setCellText(i, StepTableWidget::kColExtra, extraCellText(step));
      table->setCellText(i, StepTableWidget::kColExtra2, extra2CellText(step));
      table->setCellText(i, StepTableWidget::kColTimeout,
                         QString::number(step.timeoutMs));
      table->setStepExtData(i, step);
    }
  };

  fillTable(setup_table_, suite.setup);
  fillTable(teardown_table_, suite.teardown);

  // 删除旧的用例 tab（索引 2 及之后）
  while (tab_widget_->count() > 2) {
    QWidget* w = tab_widget_->widget(tab_widget_->count() - 1);
    tab_widget_->removeTab(tab_widget_->count() - 1);
    delete w;
  }

  // 用例 tab
  for (const auto& tc : suite.cases) {
    auto* table = new StepTableWidget(CommandTypeDelegate::Full, this);
    connectTable(table);
    fillTable(table, tc.steps);
    tab_widget_->addTab(table,
                        etest::app::AppIconProvider::instance().icon(
                            QStringLiteral("testprog_tab_case")),
                        tc.name);
  }

  // 加载完成后清选区，避免半填状态触发 onStepSelectionChanged
  setup_table_->clearSelection();
  teardown_table_->clearSelection();
  for (int i = 2; i < tab_widget_->count(); ++i) {
    if (auto* t = qobject_cast<StepTableWidget*>(tab_widget_->widget(i))) {
      t->clearSelection();
    }
  }

  tab_widget_->setCurrentIndex(0);
  rebuildVerticalTabs();
  updateActions();
}

TestProgramData TestProgramEditorWidget::uiToProgram() {
  TestProgramData suite;
  suite.version = QStringLiteral("1.1");
  suite.name = suite_name_edit_->text();
  suite.description = suite_desc_edit_->toPlainText();

  for (int i = 0; i < setup_table_->rowCount(); ++i) {
    suite.setup.append(readStepData(setup_table_, i));
  }
  for (int i = 0; i < teardown_table_->rowCount(); ++i) {
    suite.teardown.append(readStepData(teardown_table_, i));
  }

  for (int t = 2; t < tab_widget_->count(); ++t) {
    auto* table = qobject_cast<StepTableWidget*>(tab_widget_->widget(t));
    if (!table) {
      continue;
    }
    TestCaseData tc;
    tc.name = tab_widget_->tabText(t);
    for (int i = 0; i < table->rowCount(); ++i) {
      tc.steps.append(readStepData(table, i));
    }
    suite.cases.append(tc);
  }

  return suite;
}

void TestProgramEditorWidget::setModified(bool modified) {
  if (modified_ != modified) {
    modified_ = modified;
    emit modificationChanged(modified);
  }
  updateActions();
}

void TestProgramEditorWidget::resetSnapshots(bool clean) {
  snapshots_.clear();
  snapshot_index_ = -1;
  saveSnapshot();
  clean_snapshot_index_ = clean ? snapshot_index_ : -1;
}

void TestProgramEditorWidget::updateActions() {
  if (undo_action_) {
    undo_action_->setEnabled(canUndo());
  }
  if (redo_action_) {
    redo_action_->setEnabled(canRedo());
  }
  if (remove_case_action_) {
    remove_case_action_->setEnabled(tab_widget_ &&
                                    tab_widget_->currentIndex() >= 2);
  }
  if (remove_step_action_) {
    auto* table =
        tab_widget_
            ? qobject_cast<StepTableWidget*>(tab_widget_->currentWidget())
            : nullptr;
    remove_step_action_->setEnabled(table && table->currentRow() >= 0);
  }
  if (move_up_action_ || move_down_action_) {
    auto* table =
        tab_widget_
            ? qobject_cast<StepTableWidget*>(tab_widget_->currentWidget())
            : nullptr;
    int row = table ? table->currentRow() : -1;
    if (move_up_action_) {
      move_up_action_->setEnabled(table && row > 0);
    }
    if (move_down_action_) {
      move_down_action_->setEnabled(table && row >= 0 &&
                                    row < table->rowCount() - 1);
    }
  }
}

void TestProgramEditorWidget::reloadToolbarIcons() {
  auto icon = [](const QString& name) {
    return etest::app::AppIconProvider::instance().icon(name);
  };
  add_case_action_->setIcon(icon(QStringLiteral("testprog_add_case")));
  remove_case_action_->setIcon(icon(QStringLiteral("testprog_remove_case")));
  add_step_action_->setIcon(icon(QStringLiteral("testprog_add_step")));
  remove_step_action_->setIcon(icon(QStringLiteral("testprog_remove_step")));
  move_up_action_->setIcon(icon(QStringLiteral("testprog_move_up")));
  move_down_action_->setIcon(icon(QStringLiteral("testprog_move_down")));
  undo_action_->setIcon(icon(QStringLiteral("undo")));
  redo_action_->setIcon(icon(QStringLiteral("redo")));
  if (toggle_orientation_action_) {
    bool vertical = toggle_orientation_action_->isChecked();
    toggle_orientation_action_->setIcon(
        icon(vertical ? QStringLiteral("testprog_tab_horizontal")
                      : QStringLiteral("testprog_tab_vertical")));
  }
  reloadTabIcons();
}

// ── 纵向标签栏 ──

void TestProgramEditorWidget::rebuildVerticalTabs() {
  if (!vertical_tabs_model_) {
    return;
  }
  vertical_tabs_model_->clear();
  for (int i = 0; i < tab_widget_->count(); ++i) {
    auto* item = new QStandardItem;
    item->setIcon(tab_widget_->tabIcon(i));
    item->setText(tab_widget_->tabText(i));
    item->setEditable(false);
    item->setData(i, VerticalTabRole::TabIndexRole);
    item->setData(i >= 2, VerticalTabRole::ClosableRole);
    vertical_tabs_model_->appendRow(item);
  }
  int cur = tab_widget_->currentIndex();
  if (cur >= 0 && cur < vertical_tabs_model_->rowCount()) {
    syncing_vertical_tabs_ = true;
    vertical_tabs_view_->setCurrentIndex(vertical_tabs_model_->index(cur, 0));
    syncing_vertical_tabs_ = false;
  }
}

void TestProgramEditorWidget::applyTabOrientation(bool vertical) {
  if (applying_orientation_) {
    return;
  }
  applying_orientation_ = true;

  vertical_tabs_dock_->setVisible(vertical);
  // 隐藏 tabBar 时压高为 0，避免残留间隙
  auto* bar = tab_widget_->tabBar();
  if (vertical) {
    bar->setVisible(false);
    bar->setFixedHeight(0);
  } else {
    bar->setMaximumHeight(QWIDGETSIZE_MAX);
    bar->setMinimumHeight(0);
    bar->setVisible(true);
  }

  if (toggle_orientation_action_) {
    QSignalBlocker blocker(toggle_orientation_action_);
    toggle_orientation_action_->setChecked(vertical);
    toggle_orientation_action_->setIcon(
        etest::app::AppIconProvider::instance().icon(
            vertical ? QStringLiteral("testprog_tab_horizontal")
                     : QStringLiteral("testprog_tab_vertical")));
  }

  etest::core::config::ConfigManager::instance().set(
      etest::core::config::CONFIG_TEST_PROGRAM_TAB_ORIENTATION,
      vertical ? QStringLiteral("vertical") : QStringLiteral("horizontal"));

  applying_orientation_ = false;
}

void TestProgramEditorWidget::removeCaseAt(int index, bool confirm) {
  if (index < 2 || index >= tab_widget_->count()) {
    return;
  }
  if (confirm) {
    int ret = QMessageBox::question(this, QStringLiteral("删除用例"),
                                    QStringLiteral("确定删除当前测试用例？"),
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
      return;
    }
  }

  QWidget* w = tab_widget_->widget(index);
  tab_widget_->removeTab(index);
  delete w;

  rebuildVerticalTabs();
  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::renameCase(int index) {
  if (index < 2 || index >= tab_widget_->count()) {
    return;
  }
  bool ok;
  QString newName = QInputDialog::getText(
      this, QStringLiteral("重命名用例"), QStringLiteral("用例名称:"),
      QLineEdit::Normal, tab_widget_->tabText(index), &ok);
  if (ok && !newName.trimmed().isEmpty()) {
    tab_widget_->setTabText(index, newName.trimmed());
    rebuildVerticalTabs();
    saveSnapshot();
    setModified(true);
    updateActions();
  }
}

void TestProgramEditorWidget::reloadTabIcons() {
  if (!tab_widget_) {
    return;
  }
  auto icon = [](const QString& name) {
    return etest::app::AppIconProvider::instance().icon(name);
  };
  if (tab_widget_->count() > 0) {
    tab_widget_->setTabIcon(0, icon(QStringLiteral("testprog_tab_init")));
  }
  if (tab_widget_->count() > 1) {
    tab_widget_->setTabIcon(1, icon(QStringLiteral("testprog_tab_cleanup")));
  }
  for (int i = 2; i < tab_widget_->count(); ++i) {
    tab_widget_->setTabIcon(i, icon(QStringLiteral("testprog_tab_case")));
  }
  rebuildVerticalTabs();
}

}  // namespace etest::app
