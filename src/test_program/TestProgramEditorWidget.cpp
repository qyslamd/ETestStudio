#include "TestProgramEditorWidget.h"

#include <QAction>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QInputDialog>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QDockWidget>
#include <QWidget>

#include "CommandTypeDelegate.h"
#include "StepDetailPanel.h"
#include "StepValidation.h"
#include "libui/dock_title_bar/DockTitleBar.h"

namespace etest::app {

// ── 列索引常量（相对于各命令类型的列布局） ──
enum StepCol {
  kColDesc = 0,       // 步骤说明
  kColCmd = 1,        // 命令
  kColTarget = 2,     // 目标 / 条件目标 / 期望值
  kColValue = 3,      // 值 / 运算符 / 故障类型 / 期望值
  kColExtra = 4,      // 延迟ms / 容差min / 循环次数 / 条件值
  kColExtra2 = 5,     // 超时ms / 容差max / 间隔ms / 故障值
  kColTimeout = 6,    // 超时ms
  kStepColumnCount = 7
};

// ── subSteps 序列化辅助（QVector<TestStepData> → JSON → QByteArray） ──
// QVector<TestStepData> 无法直接存入 QVariant，序列化为 JSON 字符串绕开此限制
static QJsonArray subStepsToJsonArray(const QVector<TestStepData>& steps) {
  QJsonArray arr;
  for (const auto& s : steps) {
    arr.append(testStepToJson(s));
  }
  return arr;
}

static QVector<TestStepData> subStepsFromJsonArray(const QJsonArray& arr) {
  QVector<TestStepData> steps;
  for (const auto& v : arr) {
    steps.append(testStepFromJson(v.toObject()));
  }
  return steps;
}

static QByteArray serializeSubSteps(const QVector<TestStepData>& steps) {
  return QJsonDocument(subStepsToJsonArray(steps)).toJson(QJsonDocument::Compact);
}

static QVector<TestStepData> deserializeSubSteps(const QByteArray& data) {
  QJsonDocument doc = QJsonDocument::fromJson(data);
  if (doc.isArray()) {
    return subStepsFromJsonArray(doc.array());
  }
  return {};
}

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

void TestProgramEditorWidget::initUi() {
  setAutoFillBackground(true);

  // ── QToolBar ──
  auto* toolbar = addToolBar(QStringLiteral("测试程序工具栏"));
  toolbar->setObjectName(QStringLiteral("testProgramToolbar"));
  toolbar->setMovable(false);
  toolbar->setFloatable(false);

  add_case_action_ = new QAction(QStringLiteral("添加用例"), this);
  add_case_action_->setToolTip(QStringLiteral("添加测试用例"));
  toolbar->addAction(add_case_action_);

  remove_case_action_ = new QAction(QStringLiteral("删除用例"), this);
  remove_case_action_->setToolTip(QStringLiteral("删除当前测试用例"));
  toolbar->addAction(remove_case_action_);

  toolbar->addSeparator();

  add_step_action_ = new QAction(QStringLiteral("添加步骤"), this);
  add_step_action_->setToolTip(QStringLiteral("添加测试步骤"));
  toolbar->addAction(add_step_action_);

  remove_step_action_ = new QAction(QStringLiteral("删除步骤"), this);
  remove_step_action_->setToolTip(QStringLiteral("删除当前测试步骤"));
  toolbar->addAction(remove_step_action_);

  move_up_action_ = new QAction(QStringLiteral("上移"), this);
  move_up_action_->setToolTip(QStringLiteral("上移步骤"));
  toolbar->addAction(move_up_action_);

  move_down_action_ = new QAction(QStringLiteral("下移"), this);
  move_down_action_->setToolTip(QStringLiteral("下移步骤"));
  toolbar->addAction(move_down_action_);

  toolbar->addSeparator();

  undo_action_ = new QAction(QStringLiteral("撤销"), this);
  undo_action_->setShortcut(QKeySequence::Undo);
  undo_action_->setToolTip(QStringLiteral("撤销 (Ctrl+Z)"));
  toolbar->addAction(undo_action_);

  redo_action_ = new QAction(QStringLiteral("重做"), this);
  redo_action_->setShortcut(QKeySequence::Redo);
  redo_action_->setToolTip(QStringLiteral("重做 (Ctrl+Y)"));
  toolbar->addAction(redo_action_);

  auto* content = new QWidget(this);
  auto* main_layout = new QVBoxLayout(content);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  // ── Central：步骤表格 + 校验状态 ──
  tab_widget_ = new QTabWidget(content);
  tab_widget_->tabBar()->installEventFilter(this);

  setup_table_ = createStepTable(CommandTypeDelegate::Full);
  tab_widget_->addTab(setup_table_, QStringLiteral("Setup"));

  teardown_table_ = createStepTable(CommandTypeDelegate::Full);
  tab_widget_->addTab(teardown_table_, QStringLiteral("Teardown"));

  main_layout->addWidget(tab_widget_, 1);

  // ── 校验状态栏 ──
  validation_label_ = new QLabel(content);
  validation_label_->setObjectName(QStringLiteral("testProgramValidationLabel"));
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
  updateActions();
}

QTableWidget* TestProgramEditorWidget::createStepTable(
    CommandTypeDelegate::Mode delegateMode) {
  auto* table = new QTableWidget(0, kStepColumnCount, this);
  table->setHorizontalHeaderLabels(
      {QStringLiteral("步骤说明"), QStringLiteral("命令"),
       QStringLiteral("目标"), QStringLiteral("值"),
       QStringLiteral("延迟(ms)"), QStringLiteral(""), QStringLiteral("超时(ms)")});

  // 表头拉伸策略
  table->horizontalHeader()->setStretchLastSection(false);
  table->horizontalHeader()->setSectionResizeMode(kColDesc, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(kColCmd, QHeaderView::Fixed);
  table->setColumnWidth(kColCmd, 120);
  table->horizontalHeader()->setSectionResizeMode(
      kColExtra, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(
      kColExtra2, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(
      kColTimeout, QHeaderView::ResizeToContents);

  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->verticalHeader()->setDefaultSectionSize(24);
  table->setAlternatingRowColors(true);

  // 拖拽排序
  table->setDragDropMode(QAbstractItemView::InternalMove);
  table->setDragEnabled(true);
  table->setAcceptDrops(true);
  table->setDropIndicatorShown(true);
  table->setDragDropOverwriteMode(false);

  // 命令列 ComboBox 委托
  auto* cmdDelegate = new CommandTypeDelegate(delegateMode, this);
  table->setItemDelegateForColumn(kColCmd, cmdDelegate);

  connect(cmdDelegate, &CommandTypeDelegate::commandChanged, this,
          [this, table](const QString&, const QString&, const QModelIndex& idx) {
            if (loading_ || undo_redo_in_progress_) {
              return;
            }
            updateColumnHeadersForCommand(table, idx.row());
            onStepSelectionChanged();  // 刷新详情面板
            saveSnapshot();
            setModified(true);
            updateActions();
          });

  return table;
}

void TestProgramEditorWidget::connectTableSignals(QTableWidget* table) {
  connect(table, &QTableWidget::cellChanged, this,
          &TestProgramEditorWidget::onDataChanged);
  connect(table, &QTableWidget::itemSelectionChanged, this,
          &TestProgramEditorWidget::onStepSelectionChanged);
  connect(table, &QTableWidget::itemSelectionChanged, this,
          &TestProgramEditorWidget::updateActions);
  // 拖拽完成后重新编号
  connect(table->model(), &QAbstractItemModel::rowsInserted, this,
          [this, table]() {
            if (!loading_ && !undo_redo_in_progress_) {
              renumberSteps(table);
            }
          });
  connect(table->model(), &QAbstractItemModel::rowsRemoved, this,
          [this, table]() {
            if (!loading_ && !undo_redo_in_progress_) {
              renumberSteps(table);
            }
          });
}

void TestProgramEditorWidget::initSignals() {
  connect(suite_name_edit_, &QLineEdit::textChanged, this,
          &TestProgramEditorWidget::onDataChanged);
  connect(suite_desc_edit_, &QTextEdit::textChanged, this,
          &TestProgramEditorWidget::onDataChanged);

  connectTableSignals(setup_table_);
  connectTableSignals(teardown_table_);

  connect(tab_widget_, &QTabWidget::currentChanged, this, [this](int) {
    // 切换 tab 时刷新校验状态
    updateActions();
    validateCurrentTable();
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

  // 详情面板数据变更 → 同步子步骤到当前行的 UserRole
  connect(step_detail_panel_, &StepDetailPanel::dataChanged, this, [this]() {
    if (loading_ || undo_redo_in_progress_) {
      return;
    }
    // 将 detail panel 中的 subSteps 写回当前行的 UserRole
    auto* table = qobject_cast<QTableWidget*>(tab_widget_->currentWidget());
    int row = table ? table->currentRow() : -1;
    if (table && row >= 0) {
      QTableWidgetItem* cmdItem = table->item(row, kColCmd);
      if (cmdItem) {
        TestStepData panelStep = step_detail_panel_->stepData();
        if (panelStep.isControlFlow()) {
          TestStepData existing = loadStepExtData(cmdItem);
          existing.subSteps = panelStep.subSteps;
          existing.elseSubSteps = panelStep.elseSubSteps;
          storeStepExtData(cmdItem, existing);
        }
      }
    }
    saveSnapshot();
    setModified(true);
    updateActions();
  });
}

// ── 动态列头 ──

// 返回每个命令类型的列映射
static void getColumnMapping(const QString& cmd, const char** labels) {
  // 初始化为隐藏
  for (int i = 0; i < kStepColumnCount; ++i) {
    labels[i] = nullptr;
  }

  labels[kColDesc] = "步骤说明";
  labels[kColCmd] = "命令";

  if (cmd == QStringLiteral("SET")) {
    labels[kColTarget] = "目标";
    labels[kColValue] = "值";
    labels[kColExtra] = "延迟(ms)";
    labels[kColTimeout] = "超时(ms)";
  } else if (cmd == QStringLiteral("VERIFY")) {
    labels[kColTarget] = "目标";
    labels[kColValue] = "期望值";
    labels[kColExtra] = "容差min";
    labels[kColExtra2] = "容差max";
    labels[kColTimeout] = "超时(ms)";
  } else if (cmd == QStringLiteral("WAIT")) {
    labels[kColTarget] = "条件目标";
    labels[kColValue] = "运算符";
    labels[kColExtra] = "条件值";
    labels[kColExtra2] = "间隔(ms)";
    labels[kColTimeout] = "超时(ms)";
  } else if (cmd == QStringLiteral("DELAY")) {
    labels[kColExtra] = "延迟值(ms)";
  } else if (cmd == QStringLiteral("ACTION")) {
    labels[kColTarget] = "提示信息";
  } else if (cmd == QStringLiteral("LOG")) {
    labels[kColTarget] = "日志内容";
  } else if (cmd == QStringLiteral("LOOP")) {
    labels[kColTarget] = "循环次数";
    labels[kColValue] = "间隔(ms)";
  } else if (cmd == QStringLiteral("WHILE")) {
    labels[kColTarget] = "条件目标";
    labels[kColValue] = "运算符";
    labels[kColExtra] = "条件值";
    labels[kColExtra2] = "间隔(ms)";
    labels[kColTimeout] = "超时(ms)";
  } else if (cmd == QStringLiteral("IF")) {
    labels[kColTarget] = "条件目标";
    labels[kColValue] = "运算符";
    labels[kColExtra] = "条件值";
  } else if (cmd == QStringLiteral("INJECT_FAULT")) {
    labels[kColTarget] = "目标";
    labels[kColValue] = "故障类型";
    labels[kColExtra] = "故障值";
  } else if (cmd == QStringLiteral("CLEAR_FAULT")) {
    labels[kColTarget] = "目标";
  } else if (cmd == QStringLiteral("PHOTO")) {
    // 只保留 步骤说明 + 命令
  } else if (cmd == QStringLiteral("RECORD")) {
    labels[kColTarget] = "录制";
  }
}

void TestProgramEditorWidget::updateColumnHeadersForCommand(
    QTableWidget* table, int row) {
  if (row < 0 || row >= table->rowCount()) {
    return;
  }

  QTableWidgetItem* cmdItem = table->item(row, kColCmd);
  QString cmd = cmdItem ? cmdItem->text().trimmed().toUpper() : QString();
  if (cmd.isEmpty()) {
    return;
  }

  const char* labels[kStepColumnCount] = {nullptr};
  getColumnMapping(cmd, labels);

  for (int c = 0; c < kStepColumnCount; ++c) {
    if (labels[c] != nullptr) {
      table->setHorizontalHeaderItem(c, new QTableWidgetItem(labels[c]));
      table->setColumnHidden(c, false);
    } else if (c != kColDesc && c != kColCmd) {
      // 隐藏不相关的列（保留 步骤说明 和 命令 始终可见）
      table->setColumnHidden(c, true);
    }
  }
}

// ── 数据变更 ──

void TestProgramEditorWidget::onDataChanged() {
  if (loading_ || undo_redo_in_progress_) {
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

  int index = tab_widget_->count();
  auto* table = createStepTable(CommandTypeDelegate::Full);
  connectTableSignals(table);
  QString caseName = QStringLiteral("测试用例 %1").arg(index - 1);
  tab_widget_->addTab(table, caseName);
  tab_widget_->setCurrentWidget(table);

  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onRemoveCase() {
  int index = tab_widget_->currentIndex();
  if (index < 2) {
    return;
  }

  int ret = QMessageBox::question(this, QStringLiteral("删除用例"),
                                  QStringLiteral("确定删除当前测试用例？"),
                                  QMessageBox::Yes | QMessageBox::No);
  if (ret != QMessageBox::Yes) {
    return;
  }

  QWidget* w = tab_widget_->widget(index);
  tab_widget_->removeTab(index);
  delete w;

  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onAddStep() {
  if (loading_) {
    return;
  }

  auto* table = qobject_cast<QTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    return;
  }

  int row = table->rowCount();
  table->setRowCount(row + 1);

  bool wasBlocked = table->signalsBlocked();
  table->blockSignals(true);

  table->setItem(row, kColDesc, new QTableWidgetItem(QString()));
  auto* cmdItem = new QTableWidgetItem(QStringLiteral("SET"));
  cmdItem->setData(kStepDataRole, QVariantMap());
  table->setItem(row, kColCmd, cmdItem);
  table->setItem(row, kColTarget, new QTableWidgetItem(QString()));
  table->setItem(row, kColValue, new QTableWidgetItem(QString()));
  table->setItem(row, kColExtra, new QTableWidgetItem(QStringLiteral("0")));
  table->setItem(row, kColExtra2, new QTableWidgetItem(QString()));
  table->setItem(row, kColTimeout, new QTableWidgetItem(QStringLiteral("5000")));

  table->blockSignals(wasBlocked);

  updateColumnHeadersForCommand(table, row);
  renumberSteps(table);
  table->selectRow(row);

  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onRemoveStep() {
  auto* table = qobject_cast<QTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    return;
  }

  int row = table->currentRow();
  if (row < 0) {
    return;
  }

  table->removeRow(row);
  renumberSteps(table);
  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onMoveUp() {
  auto* table = qobject_cast<QTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    return;
  }

  int row = table->currentRow();
  if (row <= 0) {
    return;
  }

  // 通过拖拽行的方式交换
  // 保存当前行数据，移除，在 row-1 处插入
  QVector<QTableWidgetItem*> items(kStepColumnCount);
  for (int c = 0; c < kStepColumnCount; ++c) {
    items[c] = table->takeItem(row, c);
  }
  table->removeRow(row);
  table->insertRow(row - 1);
  for (int c = 0; c < kStepColumnCount; ++c) {
    table->setItem(row - 1, c, items[c]);
  }

  renumberSteps(table);
  table->selectRow(row - 1);
  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onMoveDown() {
  auto* table = qobject_cast<QTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    return;
  }

  int row = table->currentRow();
  if (row < 0 || row >= table->rowCount() - 1) {
    return;
  }

  // 与下行交换
  QVector<QTableWidgetItem*> items(kStepColumnCount);
  for (int c = 0; c < kStepColumnCount; ++c) {
    items[c] = table->takeItem(row, c);
  }
  table->removeRow(row);
  table->insertRow(row + 1);
  for (int c = 0; c < kStepColumnCount; ++c) {
    table->setItem(row + 1, c, items[c]);
  }

  renumberSteps(table);
  table->selectRow(row + 1);
  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::renumberSteps(QTableWidget* table) {
  for (int i = 0; i < table->rowCount(); ++i) {
    table->verticalHeader()->setSectionHidden(i, false);
    // 通过 model 设置行号
    table->model()->setHeaderData(i, Qt::Vertical, QString::number(i + 1),
                                  Qt::DisplayRole);
  }
}

void TestProgramEditorWidget::onStepSelectionChanged() {
  auto* table = qobject_cast<QTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    step_detail_panel_->clear();
    return;
  }

  int row = table->currentRow();
  if (row < 0) {
    step_detail_panel_->clear();
    return;
  }

  QString cmd;
  QTableWidgetItem* cmdItem = table->item(row, kColCmd);
  if (cmdItem) {
    cmd = cmdItem->text().trimmed().toUpper();
  }

  // 从表格当前行构建 TestStepData
  TestStepData step;
  step.cmd = cmd;
  if (table->item(row, kColDesc)) {
    step.description = table->item(row, kColDesc)->text();
  }
  if (table->item(row, kColTarget)) {
    step.target = table->item(row, kColTarget)->text();
  }
  if (table->item(row, kColValue)) {
    step.value = table->item(row, kColValue)->text();
  }

  // 从 UserRole 加载扩展数据
  if (cmdItem && cmdItem->data(kStepDataRole).isValid()) {
    QVariantMap ext = cmdItem->data(kStepDataRole).toMap();
    if (ext.contains("conditionTarget")) {
      step.condition.target = ext["conditionTarget"].toString();
    }
    if (ext.contains("conditionOp")) {
      step.condition.op = ext["conditionOp"].toString();
    }
    if (ext.contains("conditionValue")) {
      step.condition.value = ext["conditionValue"];
    }
    if (ext.contains("tolMin")) {
      step.tolerance.min = ext["tolMin"].toDouble();
    }
    if (ext.contains("tolMax")) {
      step.tolerance.max = ext["tolMax"].toDouble();
    }
    if (ext.contains("tolEnabled")) {
      step.tolerance.enabled = ext["tolEnabled"].toBool();
    }
    if (ext.contains("faultType")) {
      step.fault.type = ext["faultType"].toString();
    }
    if (ext.contains("faultValue")) {
      step.fault.value = ext["faultValue"];
    }
    if (ext.contains("loopCount")) {
      step.loopCount = ext["loopCount"].toInt();
    }
    if (ext.contains("loopIntervalMs")) {
      step.loopIntervalMs = ext["loopIntervalMs"].toInt();
    }
    if (ext.contains("subStepsJson")) {
      step.subSteps = deserializeSubSteps(ext["subStepsJson"].toString().toUtf8());
    }
    if (ext.contains("elseSubStepsJson")) {
      step.elseSubSteps = deserializeSubSteps(ext["elseSubStepsJson"].toString().toUtf8());
    }
  }

  // 始终填充面板，由面板内部根据命令类型切换页面
  step_detail_panel_->setStepData(step, false);
}

// ── 校验 ──

void TestProgramEditorWidget::validateCurrentTable() {
  auto* table = qobject_cast<QTableWidget*>(tab_widget_->currentWidget());
  if (!table) {
    updateValidationLabel();
    return;
  }

  // 校验当前表格中的所有可见行
  int errors = 0;
  int warnings = 0;
  QStringList details;

  for (int i = 0; i < table->rowCount(); ++i) {
    QTableWidgetItem* cmdItem = table->item(i, kColCmd);
    if (!cmdItem) {
      continue;
    }

    TestStepData step = loadStepExtData(cmdItem);
    step.cmd = table->item(i, kColCmd)
                   ? table->item(i, kColCmd)->text()
                   : QString();
    step.description = table->item(i, kColDesc)
                           ? table->item(i, kColDesc)->text()
                           : QString();
    step.target = table->item(i, kColTarget)
                      ? table->item(i, kColTarget)->text()
                      : QString();
    step.value = table->item(i, kColValue)
                     ? QVariant(table->item(i, kColValue)->text())
                     : QVariant();
    step.delayMs =
        table->item(i, kColExtra) ? table->item(i, kColExtra)->text().toInt() : 0;
    step.timeoutMs = table->item(i, kColTimeout)
                         ? table->item(i, kColTimeout)->text().toInt()
                         : 5000;

    QStringList issues = StepValidation::validateStep(step);
    if (!issues.isEmpty()) {
      // 行标红
      for (int c = 0; c < kStepColumnCount; ++c) {
        if (table->item(i, c)) {
          table->item(i, c)->setBackground(QColor(80, 30, 30));
          table->item(i, c)->setToolTip(issues.join(QStringLiteral("\n")));
        }
      }
      // 统计严重性
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
      // 清除错误样式
      for (int c = 0; c < kStepColumnCount; ++c) {
        if (table->item(i, c)) {
          table->item(i, c)->setBackground(QColor());
          table->item(i, c)->setToolTip(QString());
        }
      }
    }
  }

  if (errors > 0 || warnings > 0) {
    // 显示摘要 + 首条错误详情
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
    // 附加第一条具体错误便于快速定位
    if (!details.isEmpty()) {
      labelText += QStringLiteral(" · %1").arg(details.first());
    }
    // 全部错误存入 tooltip
    validation_label_->setText(labelText);
    validation_label_->setToolTip(details.join(QStringLiteral("\n")));
    validation_label_->setVisible(true);
  } else {
    validation_label_->setVisible(false);
  }
}

void TestProgramEditorWidget::updateValidationLabel() {
  // 简单版：仅隐藏标签
  validation_label_->setVisible(false);
}

// ── 扩展数据存取 ──

void TestProgramEditorWidget::storeStepExtData(QTableWidgetItem* item,
                                                const TestStepData& step) {
  QVariantMap ext;
  if (!step.condition.target.isEmpty()) {
    ext["conditionTarget"] = step.condition.target;
    ext["conditionOp"] = step.condition.op;
    ext["conditionValue"] = step.condition.value;
  }
  if (step.tolerance.enabled) {
    ext["tolMin"] = step.tolerance.min;
    ext["tolMax"] = step.tolerance.max;
    ext["tolEnabled"] = step.tolerance.enabled;
  }
  if (!step.fault.type.isEmpty()) {
    ext["faultType"] = step.fault.type;
    ext["faultValue"] = step.fault.value;
  }
  if (step.loopCount > 1 || step.loopIntervalMs > 0) {
    ext["loopCount"] = step.loopCount;
    ext["loopIntervalMs"] = step.loopIntervalMs;
  }
  // subSteps/elseSubSteps 序列化存入 UserRole（避免多控制流步骤时数据丢失）
  if (!step.subSteps.isEmpty()) {
    ext["subStepsJson"] = QString::fromUtf8(serializeSubSteps(step.subSteps));
  }
  if (!step.elseSubSteps.isEmpty()) {
    ext["elseSubStepsJson"] = QString::fromUtf8(serializeSubSteps(step.elseSubSteps));
  }
  item->setData(kStepDataRole, ext);
}

TestStepData TestProgramEditorWidget::loadStepExtData(
    const QTableWidgetItem* item) const {
  TestStepData step;
  if (!item || !item->data(kStepDataRole).isValid()) {
    return step;
  }
  QVariantMap ext = item->data(kStepDataRole).toMap();
  if (ext.contains("conditionTarget")) {
    step.condition.target = ext["conditionTarget"].toString();
  }
  if (ext.contains("conditionOp")) {
    step.condition.op = ext["conditionOp"].toString();
  }
  if (ext.contains("conditionValue")) {
    step.condition.value = ext["conditionValue"];
  }
  if (ext.contains("tolMin")) {
    step.tolerance.min = ext["tolMin"].toDouble();
  }
  if (ext.contains("tolMax")) {
    step.tolerance.max = ext["tolMax"].toDouble();
  }
  if (ext.contains("tolEnabled")) {
    step.tolerance.enabled = ext["tolEnabled"].toBool();
  }
  if (ext.contains("faultType")) {
    step.fault.type = ext["faultType"].toString();
  }
  if (ext.contains("faultValue")) {
    step.fault.value = ext["faultValue"];
  }
  if (ext.contains("loopCount")) {
    step.loopCount = ext["loopCount"].toInt();
  }
  if (ext.contains("loopIntervalMs")) {
    step.loopIntervalMs = ext["loopIntervalMs"].toInt();
  }
  if (ext.contains("subStepsJson")) {
    step.subSteps = deserializeSubSteps(ext["subStepsJson"].toString().toUtf8());
  }
  if (ext.contains("elseSubStepsJson")) {
    step.elseSubSteps = deserializeSubSteps(ext["elseSubStepsJson"].toString().toUtf8());
  }
  return step;
}

// ── 事件过滤（Tab 双击重命名） ──

bool TestProgramEditorWidget::eventFilter(QObject* obj, QEvent* event) {
  if (obj == tab_widget_->tabBar() && event->type() == QEvent::MouseButtonDblClick) {
    auto* me = static_cast<QMouseEvent*>(event);
    int tabIdx = tab_widget_->tabBar()->tabAt(me->pos());
    if (tabIdx >= 2) {
      // 内联重命名
      tab_widget_->tabBar()->setCurrentIndex(tabIdx);
      // 简单实现：弹出 QInputDialog
      bool ok;
      QString newName = QInputDialog::getText(
          this, QStringLiteral("重命名用例"),
          QStringLiteral("用例名称:"), QLineEdit::Normal,
          tab_widget_->tabText(tabIdx), &ok);
      if (ok && !newName.trimmed().isEmpty()) {
        tab_widget_->setTabText(tabIdx, newName.trimmed());
        saveSnapshot();
        setModified(true);
        updateActions();
      }
      return true;
    }
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
  loading_ = true;
  loadProgramToUi(state);
  loading_ = false;

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

  // Setup 步骤
  setup_table_->setRowCount(suite.setup.size());
  for (int i = 0; i < suite.setup.size(); ++i) {
    const auto& step = suite.setup[i];
    auto* cmdItem = new QTableWidgetItem(step.cmd);
    storeStepExtData(cmdItem, step);
    setup_table_->setItem(i, kColDesc, new QTableWidgetItem(step.description));
    setup_table_->setItem(i, kColCmd, cmdItem);
    setup_table_->setItem(i, kColTarget, new QTableWidgetItem(step.target));
    setup_table_->setItem(i, kColValue,
                          new QTableWidgetItem(step.value.toString()));
    setup_table_->setItem(i, kColExtra,
                          new QTableWidgetItem(QString::number(step.delayMs)));
    setup_table_->setItem(i, kColExtra2, new QTableWidgetItem(QString()));
    setup_table_->setItem(i, kColTimeout,
                          new QTableWidgetItem(QString::number(step.timeoutMs)));
    updateColumnHeadersForCommand(setup_table_, i);
  }
  renumberSteps(setup_table_);

  // Teardown 步骤
  teardown_table_->setRowCount(suite.teardown.size());
  for (int i = 0; i < suite.teardown.size(); ++i) {
    const auto& step = suite.teardown[i];
    auto* cmdItem = new QTableWidgetItem(step.cmd);
    storeStepExtData(cmdItem, step);
    teardown_table_->setItem(i, kColDesc, new QTableWidgetItem(step.description));
    teardown_table_->setItem(i, kColCmd, cmdItem);
    teardown_table_->setItem(i, kColTarget, new QTableWidgetItem(step.target));
    teardown_table_->setItem(i, kColValue,
                             new QTableWidgetItem(step.value.toString()));
    teardown_table_->setItem(i, kColExtra,
                             new QTableWidgetItem(QString::number(step.delayMs)));
    teardown_table_->setItem(i, kColExtra2, new QTableWidgetItem(QString()));
    teardown_table_->setItem(
        i, kColTimeout, new QTableWidgetItem(QString::number(step.timeoutMs)));
    updateColumnHeadersForCommand(teardown_table_, i);
  }
  renumberSteps(teardown_table_);

  // 删除旧的用例 tab（索引 2 及之后）
  while (tab_widget_->count() > 2) {
    QWidget* w = tab_widget_->widget(tab_widget_->count() - 1);
    tab_widget_->removeTab(tab_widget_->count() - 1);
    delete w;
  }

  // 用例 tab
  for (const auto& tc : suite.cases) {
    auto* table = createStepTable(CommandTypeDelegate::Full);
    connectTableSignals(table);
    table->setRowCount(tc.steps.size());
    for (int i = 0; i < tc.steps.size(); ++i) {
      const auto& step = tc.steps[i];
      auto* cmdItem = new QTableWidgetItem(step.cmd);
      storeStepExtData(cmdItem, step);
      table->setItem(i, kColDesc, new QTableWidgetItem(step.description));
      table->setItem(i, kColCmd, cmdItem);
      table->setItem(i, kColTarget, new QTableWidgetItem(step.target));
      table->setItem(i, kColValue,
                     new QTableWidgetItem(step.value.toString()));
      table->setItem(i, kColExtra,
                     new QTableWidgetItem(QString::number(step.delayMs)));
      table->setItem(i, kColExtra2, new QTableWidgetItem(QString()));
      table->setItem(
          i, kColTimeout, new QTableWidgetItem(QString::number(step.timeoutMs)));
      updateColumnHeadersForCommand(table, i);
    }
    renumberSteps(table);
    tab_widget_->addTab(table, tc.name);
  }

  // 默认选中第一个 tab
  tab_widget_->setCurrentIndex(0);
  updateActions();
}

TestProgramData TestProgramEditorWidget::uiToProgram() {
  TestProgramData suite;
  suite.version = QStringLiteral("1.1");
  suite.name = suite_name_edit_->text();
  suite.description = suite_desc_edit_->toPlainText();

  // 解析表格行 → TestStepData 的通用函数
  auto readStep = [this](QTableWidget* table, int i) -> TestStepData {
    TestStepData step;
    step.description = table->item(i, kColDesc)
                           ? table->item(i, kColDesc)->text()
                           : QString();
    step.cmd = table->item(i, kColCmd) ? table->item(i, kColCmd)->text()
                                        : QString();
    step.target = table->item(i, kColTarget) ? table->item(i, kColTarget)->text()
                                              : QString();
    step.value = table->item(i, kColValue)
                     ? QVariant(table->item(i, kColValue)->text())
                     : QVariant();
    step.delayMs =
        table->item(i, kColExtra) ? table->item(i, kColExtra)->text().toInt() : 0;
    step.timeoutMs = table->item(i, kColTimeout)
                         ? table->item(i, kColTimeout)->text().toInt()
                         : 5000;
    // 从 item cmd 加载扩展数据
    if (table->item(i, kColCmd)) {
      auto ext = table->item(i, kColCmd)->data(
          TestProgramEditorWidget::kStepDataRole);
      if (ext.isValid()) {
        QVariantMap m = ext.toMap();
        if (m.contains("conditionTarget")) {
          step.condition.target = m["conditionTarget"].toString();
          step.condition.op = m["conditionOp"].toString();
          step.condition.value = m["conditionValue"];
        }
        if (m.contains("tolEnabled")) {
          step.tolerance.enabled = m["tolEnabled"].toBool();
          step.tolerance.min = m["tolMin"].toDouble();
          step.tolerance.max = m["tolMax"].toDouble();
        }
        if (m.contains("faultType")) {
          step.fault.type = m["faultType"].toString();
          step.fault.value = m["faultValue"];
        }
        if (m.contains("loopCount")) {
          step.loopCount = m["loopCount"].toInt();
          step.loopIntervalMs = m["loopIntervalMs"].toInt();
        }
        if (m.contains("subStepsJson")) {
          step.subSteps = deserializeSubSteps(m["subStepsJson"].toString().toUtf8());
        }
        if (m.contains("elseSubStepsJson")) {
          step.elseSubSteps = deserializeSubSteps(m["elseSubStepsJson"].toString().toUtf8());
        }
      }
    }
    return step;
  };

  // Setup
  for (int i = 0; i < setup_table_->rowCount(); ++i) {
    suite.setup.append(readStep(setup_table_, i));
  }

  // Teardown
  for (int i = 0; i < teardown_table_->rowCount(); ++i) {
    suite.teardown.append(readStep(teardown_table_, i));
  }

  // 用例 tab（索引 2 开始）
  for (int t = 2; t < tab_widget_->count(); ++t) {
    auto* table = qobject_cast<QTableWidget*>(tab_widget_->widget(t));
    if (!table) {
      continue;
    }

    TestCaseData tc;
    tc.name = tab_widget_->tabText(t);
    tc.description = QString();

    for (int i = 0; i < table->rowCount(); ++i) {
      tc.steps.append(readStep(table, i));
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
    remove_case_action_->setEnabled(
        tab_widget_ && tab_widget_->currentIndex() >= 2);
  }
  if (remove_step_action_) {
    auto* table = tab_widget_
                      ? qobject_cast<QTableWidget*>(tab_widget_->currentWidget())
                      : nullptr;
    remove_step_action_->setEnabled(table && table->currentRow() >= 0);
  }
  if (move_up_action_) {
    auto* table = tab_widget_
                      ? qobject_cast<QTableWidget*>(tab_widget_->currentWidget())
                      : nullptr;
    int row = table ? table->currentRow() : -1;
    move_up_action_->setEnabled(table && row > 0);
    move_down_action_->setEnabled(table && row >= 0 &&
                                  row < table->rowCount() - 1);
  }
}

}  // namespace etest::app
