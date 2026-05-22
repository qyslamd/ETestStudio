#include "TestProgramEditorWidget.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace etest::app {

TestProgramEditorWidget::TestProgramEditorWidget(const QString& id, QWidget* parent)
    : QWidget(parent) {
  current_file_ = id;
  initUi();
  initSignals();

  if (!id.isEmpty() && !id.startsWith("editor://") && QFileInfo::exists(id)) {
    loadFile(id);
  }
}

void TestProgramEditorWidget::initUi() {
  auto* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(8, 8, 8, 8);
  main_layout->setSpacing(8);

  // ── 套件信息 ──
  auto* info_layout = new QHBoxLayout();
  info_layout->setSpacing(8);

  auto* name_label = new QLabel(QStringLiteral("套件名称:"), this);
  suite_name_edit_ = new QLineEdit(this);
  info_layout->addWidget(name_label);
  info_layout->addWidget(suite_name_edit_, 1);

  main_layout->addLayout(info_layout);

  suite_desc_edit_ = new QTextEdit(this);
  suite_desc_edit_->setPlaceholderText(QStringLiteral("套件描述..."));
  suite_desc_edit_->setMaximumHeight(60);
  main_layout->addWidget(suite_desc_edit_);

  // ── 工具栏 ──
  auto* toolbar = new QWidget(this);
  toolbar->setObjectName(QStringLiteral("testProgramEditorToolbar"));
  auto* toolbar_layout = new QHBoxLayout(toolbar);
  toolbar_layout->setContentsMargins(0, 0, 0, 0);
  toolbar_layout->setSpacing(4);

  add_case_btn_ = new QToolButton(this);
  add_case_btn_->setText(QStringLiteral("+ 添加用例"));
  add_case_btn_->setObjectName(QStringLiteral("testProgramEditorAddCaseBtn"));

  remove_case_btn_ = new QToolButton(this);
  remove_case_btn_->setText(QStringLiteral("- 删除用例"));
  remove_case_btn_->setObjectName(QStringLiteral("testProgramEditorRemoveCaseBtn"));

  toolbar_layout->addWidget(add_case_btn_);
  toolbar_layout->addWidget(remove_case_btn_);
  toolbar_layout->addStretch();

  add_step_btn_ = new QToolButton(this);
  add_step_btn_->setText(QStringLiteral("+ 添加步骤"));
  add_step_btn_->setObjectName(QStringLiteral("testProgramEditorAddStepBtn"));

  remove_step_btn_ = new QToolButton(this);
  remove_step_btn_->setText(QStringLiteral("- 删除步骤"));
  remove_step_btn_->setObjectName(QStringLiteral("testProgramEditorRemoveStepBtn"));

  toolbar_layout->addWidget(add_step_btn_);
  toolbar_layout->addWidget(remove_step_btn_);

  main_layout->addWidget(toolbar);

  // ── Tab 页：Setup / Teardown / Cases ──
  tab_widget_ = new QTabWidget(this);

  setup_table_ = createStepTable();
  tab_widget_->addTab(setup_table_, QStringLiteral("Setup"));

  teardown_table_ = createStepTable();
  tab_widget_->addTab(teardown_table_, QStringLiteral("Teardown"));

  main_layout->addWidget(tab_widget_, 1);
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
}

void TestProgramEditorWidget::initSignals() {
  connect(suite_name_edit_, &QLineEdit::textChanged, this,
          &TestProgramEditorWidget::onDataChanged);
  connect(suite_desc_edit_, &QTextEdit::textChanged, this,
          &TestProgramEditorWidget::onDataChanged);

  connectTableSignals(setup_table_);
  connectTableSignals(teardown_table_);

  connect(add_case_btn_, &QToolButton::clicked, this,
          &TestProgramEditorWidget::onAddCase);
  connect(remove_case_btn_, &QToolButton::clicked, this,
          &TestProgramEditorWidget::onRemoveCase);
  connect(add_step_btn_, &QToolButton::clicked, this,
          &TestProgramEditorWidget::onAddStep);
  connect(remove_step_btn_, &QToolButton::clicked, this,
          &TestProgramEditorWidget::onRemoveStep);
}

// ── 数据变更 ──

void TestProgramEditorWidget::onDataChanged() {
  if (loading_ || undo_redo_in_progress_) return;
  saveSnapshot();
  setModified(true);
}

void TestProgramEditorWidget::onAddCase() {
  if (loading_) return;

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
}

void TestProgramEditorWidget::onRemoveCase() {
  int index = tab_widget_->currentIndex();
  if (index < 2) return;  // 不能删除 Setup 和 Teardown

  int ret = QMessageBox::question(this, QStringLiteral("删除用例"),
                                   QStringLiteral("确定删除当前测试用例？"),
                                   QMessageBox::Yes | QMessageBox::No);
  if (ret != QMessageBox::Yes) return;

  QWidget* w = tab_widget_->widget(index);
  tab_widget_->removeTab(index);
  delete w;

  saveSnapshot();
  setModified(true);
}

void TestProgramEditorWidget::onAddStep() {
  if (loading_) return;

  auto* table = qobject_cast<QTableWidget*>(tab_widget_->currentWidget());
  if (!table) return;

  int row = table->rowCount();
  table->setRowCount(row + 1);

  // 批量插入默认值，block 信号避免每个 setItem 都触发 cellChanged
  table->blockSignals(true);
  table->setItem(row, 0, new QTableWidgetItem(QString()));
  table->setItem(row, 1, new QTableWidgetItem(QStringLiteral("SET")));
  table->setItem(row, 2, new QTableWidgetItem(QString()));
  table->setItem(row, 3, new QTableWidgetItem(QString()));
  table->setItem(row, 4, new QTableWidgetItem(QStringLiteral("0")));
  table->setItem(row, 5, new QTableWidgetItem(QStringLiteral("5000")));
  table->blockSignals(false);

  saveSnapshot();
  setModified(true);
}

void TestProgramEditorWidget::onRemoveStep() {
  auto* table = qobject_cast<QTableWidget*>(tab_widget_->currentWidget());
  if (!table) return;

  int row = table->currentRow();
  if (row < 0) return;

  table->removeRow(row);
  saveSnapshot();
  setModified(true);
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
  }
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
}

bool TestProgramEditorWidget::canUndo() const {
  return snapshot_index_ > 0;
}

bool TestProgramEditorWidget::canRedo() const {
  return snapshot_index_ < snapshots_.size() - 1;
}

void TestProgramEditorWidget::undo() {
  if (!canUndo()) return;

  undo_redo_in_progress_ = true;
  snapshot_index_--;
  restoreState(snapshots_[snapshot_index_]);
  undo_redo_in_progress_ = false;
}

void TestProgramEditorWidget::redo() {
  if (!canRedo()) return;

  undo_redo_in_progress_ = true;
  snapshot_index_++;
  restoreState(snapshots_[snapshot_index_]);
  undo_redo_in_progress_ = false;
}

// ── IEditor 接口 ──

QString TestProgramEditorWidget::displayName() const {
  return QFileInfo(current_file_).fileName();
}

bool TestProgramEditorWidget::isModified() const {
  return modified_;
}

bool TestProgramEditorWidget::save() {
  if (current_file_.isEmpty()) return false;
  return saveFile(current_file_);
}

bool TestProgramEditorWidget::saveAs(const QString& path) {
  if (path.isEmpty()) return false;
  QString oldId = current_file_;
  current_file_ = path;
  if (saveFile(path)) {
    emit editorIdChanged(oldId, current_file_);
    return true;
  }
  current_file_ = oldId;
  return false;
}

QString TestProgramEditorWidget::filePath() const {
  return current_file_;
}

QString TestProgramEditorWidget::editorId() const {
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

void TestProgramEditorWidget::setEditorId(const QString& id) {
  current_file_ = id;
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

  // 重置快照栈
  snapshots_.clear();
  snapshot_index_ = -1;
  // 保存初始状态作为撤销基底
  saveSnapshot();
  clean_snapshot_index_ = snapshot_index_;

  modified_ = false;
  return true;
}

bool TestProgramEditorWidget::saveFile(const QString& path) {
  TestProgramData suite = uiToProgram();
  if (!saveTestProgram(path, suite)) {
    return false;
  }

  // 保存后重置快照栈，将当前状态作为新的基底
  snapshots_.clear();
  snapshot_index_ = -1;
  saveSnapshot();
  clean_snapshot_index_ = snapshot_index_;

  modified_ = false;
  emit modificationChanged(false);
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
    setup_table_->setItem(i, 3,
                           new QTableWidgetItem(step.value.toString()));
    setup_table_->setItem(i, 4,
                           new QTableWidgetItem(QString::number(step.delayMs)));
    setup_table_->setItem(i, 5,
                           new QTableWidgetItem(
                               QString::number(step.timeoutMs)));
  }

  // Teardown 步骤
  teardown_table_->setRowCount(suite.teardown.size());
  for (int i = 0; i < suite.teardown.size(); ++i) {
    const auto& step = suite.teardown[i];
    teardown_table_->setItem(i, 0, new QTableWidgetItem(step.description));
    teardown_table_->setItem(i, 1, new QTableWidgetItem(step.cmd));
    teardown_table_->setItem(i, 2, new QTableWidgetItem(step.target));
    teardown_table_->setItem(i, 3,
                              new QTableWidgetItem(step.value.toString()));
    teardown_table_->setItem(i, 4,
                              new QTableWidgetItem(
                                  QString::number(step.delayMs)));
    teardown_table_->setItem(i, 5,
                              new QTableWidgetItem(
                                  QString::number(step.timeoutMs)));
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
    step.target = setup_table_->item(i, 2)
                      ? setup_table_->item(i, 2)->text()
                      : QString();
    step.value = setup_table_->item(i, 3)
                     ? QVariant(setup_table_->item(i, 3)->text())
                     : QVariant();
    step.delayMs =
        setup_table_->item(i, 4)
            ? setup_table_->item(i, 4)->text().toInt()
            : 0;
    step.timeoutMs =
        setup_table_->item(i, 5)
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
    step.delayMs =
        teardown_table_->item(i, 4)
            ? teardown_table_->item(i, 4)->text().toInt()
            : 0;
    step.timeoutMs =
        teardown_table_->item(i, 5)
            ? teardown_table_->item(i, 5)->text().toInt()
            : 5000;
    suite.teardown.append(step);
  }

  // 用例 tab（索引 2 开始）
  for (int t = 2; t < tab_widget_->count(); ++t) {
    auto* table = qobject_cast<QTableWidget*>(tab_widget_->widget(t));
    if (!table) continue;

    TestCaseData tc;
    tc.name = tab_widget_->tabText(t);
    tc.description = QString();

    for (int i = 0; i < table->rowCount(); ++i) {
      TestStepData step;
      step.description = table->item(i, 0) ? table->item(i, 0)->text()
                                            : QString();
      step.cmd =
          table->item(i, 1) ? table->item(i, 1)->text() : QString();
      step.target = table->item(i, 2) ? table->item(i, 2)->text()
                                       : QString();
      step.value = table->item(i, 3)
                       ? QVariant(table->item(i, 3)->text())
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
}

}  // namespace etest::app
