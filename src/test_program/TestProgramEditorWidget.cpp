#include "TestProgramEditorWidget.h"

#include <QAction>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace etest::app {

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
  main_layout->setContentsMargins(8, 8, 8, 8);
  main_layout->setSpacing(8);

  // ── 套件信息 ──
  auto* info_layout = new QHBoxLayout();
  info_layout->setSpacing(8);

  auto* name_label = new QLabel(QStringLiteral("套件名称:"), content);
  suite_name_edit_ = new QLineEdit(content);
  info_layout->addWidget(name_label);
  info_layout->addWidget(suite_name_edit_, 1);

  main_layout->addLayout(info_layout);

  suite_desc_edit_ = new QTextEdit(content);
  suite_desc_edit_->setPlaceholderText(QStringLiteral("套件描述..."));
  suite_desc_edit_->setMaximumHeight(60);
  main_layout->addWidget(suite_desc_edit_);

  // ── Tab 页：Setup / Teardown / Cases ──
  tab_widget_ = new QTabWidget(content);

  setup_table_ = createStepTable();
  tab_widget_->addTab(setup_table_, QStringLiteral("Setup"));

  teardown_table_ = createStepTable();
  tab_widget_->addTab(teardown_table_, QStringLiteral("Teardown"));

  main_layout->addWidget(tab_widget_, 1);
  setCentralWidget(content);

  updateActions();
}

QTableWidget* TestProgramEditorWidget::createStepTable() {
  auto* table = new QTableWidget(0, 6, this);
  table->setHorizontalHeaderLabels(
      {QStringLiteral("步骤说明"), QStringLiteral("命令"),
       QStringLiteral("目标"), QStringLiteral("值"),
       QStringLiteral("延迟(ms)"), QStringLiteral("超时(ms)")});

  table->horizontalHeader()->setStretchLastSection(true);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->verticalHeader()->setVisible(false);
  table->setAlternatingRowColors(true);

  return table;
}

void TestProgramEditorWidget::connectTableSignals(QTableWidget* table) {
  connect(table, &QTableWidget::cellChanged, this,
          &TestProgramEditorWidget::onDataChanged);
  connect(table, &QTableWidget::itemSelectionChanged, this,
          &TestProgramEditorWidget::updateActions);
}

void TestProgramEditorWidget::initSignals() {
  connect(suite_name_edit_, &QLineEdit::textChanged, this,
          &TestProgramEditorWidget::onDataChanged);
  connect(suite_desc_edit_, &QTextEdit::textChanged, this,
          &TestProgramEditorWidget::onDataChanged);

  connectTableSignals(setup_table_);
  connectTableSignals(teardown_table_);

  connect(tab_widget_, &QTabWidget::currentChanged, this,
          &TestProgramEditorWidget::updateActions);
  connect(add_case_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::onAddCase);
  connect(remove_case_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::onRemoveCase);
  connect(add_step_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::onAddStep);
  connect(remove_step_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::onRemoveStep);
  connect(undo_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::undo);
  connect(redo_action_, &QAction::triggered, this,
          &TestProgramEditorWidget::redo);
}

// ── 数据变更 ──

void TestProgramEditorWidget::onDataChanged() {
  if (loading_ || undo_redo_in_progress_) {
    return;
  }
  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onAddCase() {
  if (loading_) {
    return;
  }

  int index = tab_widget_->count();
  auto* table = createStepTable();
  connectTableSignals(table);
  QString caseName = QStringLiteral("测试用例 %1").arg(index - 1);
  tab_widget_->addTab(table, caseName);
  tab_widget_->setCurrentWidget(table);

  // 添加一行默认步骤
  table->setRowCount(table->rowCount() + 1);

  saveSnapshot();
  setModified(true);
  updateActions();
}

void TestProgramEditorWidget::onRemoveCase() {
  int index = tab_widget_->currentIndex();
  if (index < 2) {
    return;  // 不能删除 Setup 和 Teardown
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

  // 批量插入默认值，block 信号避免每个 setItem 都触发 cellChanged
  bool wasBlocked = table->signalsBlocked();
  table->blockSignals(true);
  table->setItem(row, 0, new QTableWidgetItem(QString()));
  table->setItem(row, 1, new QTableWidgetItem(QStringLiteral("SET")));
  table->setItem(row, 2, new QTableWidgetItem(QString()));
  table->setItem(row, 3, new QTableWidgetItem(QString()));
  table->setItem(row, 4, new QTableWidgetItem(QStringLiteral("0")));
  table->setItem(row, 5, new QTableWidgetItem(QStringLiteral("5000")));
  table->blockSignals(wasBlocked);
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
  saveSnapshot();
  setModified(true);
  updateActions();
}

// ── 快照式撤销/重做 ──

void TestProgramEditorWidget::saveSnapshot() {
  // 丢弃当前索引之后的所有快照
  if (snapshot_index_ < snapshots_.size() - 1) {
    snapshots_.resize(snapshot_index_ + 1);
  }
  // 保存当前 UI 状态
  snapshots_.append(uiToProgram());
  snapshot_index_ = snapshots_.size() - 1;
  // 限制快照数量
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

  // 保存后将当前状态标记为干净
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
    setup_table_->setItem(i, 0, new QTableWidgetItem(step.description));
    setup_table_->setItem(i, 1, new QTableWidgetItem(step.cmd));
    setup_table_->setItem(i, 2, new QTableWidgetItem(step.target));
    setup_table_->setItem(i, 3, new QTableWidgetItem(step.value.toString()));
    setup_table_->setItem(i, 4,
                          new QTableWidgetItem(QString::number(step.delayMs)));
    setup_table_->setItem(i, 5,
                          new QTableWidgetItem(QString::number(step.timeoutMs)));
  }

  // Teardown 步骤
  teardown_table_->setRowCount(suite.teardown.size());
  for (int i = 0; i < suite.teardown.size(); ++i) {
    const auto& step = suite.teardown[i];
    teardown_table_->setItem(i, 0, new QTableWidgetItem(step.description));
    teardown_table_->setItem(i, 1, new QTableWidgetItem(step.cmd));
    teardown_table_->setItem(i, 2, new QTableWidgetItem(step.target));
    teardown_table_->setItem(i, 3, new QTableWidgetItem(step.value.toString()));
    teardown_table_->setItem(i, 4,
                             new QTableWidgetItem(QString::number(step.delayMs)));
    teardown_table_->setItem(
        i, 5, new QTableWidgetItem(QString::number(step.timeoutMs)));
  }

  // 删除旧的用例 tab（索引 2 及之后），需手动删除 widget
  while (tab_widget_->count() > 2) {
    QWidget* w = tab_widget_->widget(tab_widget_->count() - 1);
    tab_widget_->removeTab(tab_widget_->count() - 1);
    delete w;
  }

  // 用例 tab
  for (const auto& tc : suite.cases) {
    auto* table = createStepTable();
    connectTableSignals(table);
    table->setRowCount(tc.steps.size());
    for (int i = 0; i < tc.steps.size(); ++i) {
      const auto& step = tc.steps[i];
      table->setItem(i, 0, new QTableWidgetItem(step.description));
      table->setItem(i, 1, new QTableWidgetItem(step.cmd));
      table->setItem(i, 2, new QTableWidgetItem(step.target));
      table->setItem(i, 3, new QTableWidgetItem(step.value.toString()));
      table->setItem(i, 4,
                     new QTableWidgetItem(QString::number(step.delayMs)));
      table->setItem(i, 5,
                     new QTableWidgetItem(QString::number(step.timeoutMs)));
    }
    tab_widget_->addTab(table, tc.name);
  }

  updateActions();
}

TestProgramData TestProgramEditorWidget::uiToProgram() const {
  TestProgramData suite;
  suite.version = QStringLiteral("1.0");
  suite.name = suite_name_edit_->text();
  suite.description = suite_desc_edit_->toPlainText();

  // Setup
  for (int i = 0; i < setup_table_->rowCount(); ++i) {
    TestStepData step;
    step.description = setup_table_->item(i, 0)
                           ? setup_table_->item(i, 0)->text()
                           : QString();
    step.cmd = setup_table_->item(i, 1) ? setup_table_->item(i, 1)->text()
                                        : QString();
    step.target = setup_table_->item(i, 2) ? setup_table_->item(i, 2)->text()
                                           : QString();
    step.value = setup_table_->item(i, 3)
                     ? QVariant(setup_table_->item(i, 3)->text())
                     : QVariant();
    step.delayMs =
        setup_table_->item(i, 4) ? setup_table_->item(i, 4)->text().toInt() : 0;
    step.timeoutMs = setup_table_->item(i, 5)
                         ? setup_table_->item(i, 5)->text().toInt()
                         : 5000;
    suite.setup.append(step);
  }

  // Teardown
  for (int i = 0; i < teardown_table_->rowCount(); ++i) {
    TestStepData step;
    step.description = teardown_table_->item(i, 0)
                           ? teardown_table_->item(i, 0)->text()
                           : QString();
    step.cmd = teardown_table_->item(i, 1)
                   ? teardown_table_->item(i, 1)->text()
                   : QString();
    step.target = teardown_table_->item(i, 2)
                      ? teardown_table_->item(i, 2)->text()
                      : QString();
    step.value = teardown_table_->item(i, 3)
                     ? QVariant(teardown_table_->item(i, 3)->text())
                     : QVariant();
    step.delayMs = teardown_table_->item(i, 4)
                       ? teardown_table_->item(i, 4)->text().toInt()
                       : 0;
    step.timeoutMs = teardown_table_->item(i, 5)
                         ? teardown_table_->item(i, 5)->text().toInt()
                         : 5000;
    suite.teardown.append(step);
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
      TestStepData step;
      step.description = table->item(i, 0) ? table->item(i, 0)->text()
                                           : QString();
      step.cmd = table->item(i, 1) ? table->item(i, 1)->text() : QString();
      step.target = table->item(i, 2) ? table->item(i, 2)->text() : QString();
      step.value = table->item(i, 3) ? QVariant(table->item(i, 3)->text())
                                     : QVariant();
      step.delayMs =
          table->item(i, 4) ? table->item(i, 4)->text().toInt() : 0;
      step.timeoutMs =
          table->item(i, 5) ? table->item(i, 5)->text().toInt() : 5000;
      tc.steps.append(step);
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
    remove_case_action_->setEnabled(tab_widget_ && tab_widget_->currentIndex() >= 2);
  }
  if (remove_step_action_) {
    auto* table = tab_widget_ ? qobject_cast<QTableWidget*>(tab_widget_->currentWidget())
                              : nullptr;
    remove_step_action_->setEnabled(table && table->currentRow() >= 0);
  }
}

}  // namespace etest::app
