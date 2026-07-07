#include "sub_step_table_widget.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "CommandTypeDelegate.h"

namespace etest::app {

namespace {
constexpr int kColDesc = 0;
constexpr int kColCmd = 1;
constexpr int kColTarget = 2;
constexpr int kColValue = 3;
constexpr int kColDelay = 4;
constexpr int kColCount = 5;
}  // namespace

SubStepTableWidget::SubStepTableWidget(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
}

void SubStepTableWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setSpacing(4);
  layout->setContentsMargins(0, 0, 0, 0);

  // 工具栏：+/- 靠左，↑/↓ 靠右
  auto* toolbar = new QHBoxLayout();
  toolbar->setSpacing(2);
  add_btn_ = new QPushButton(QStringLiteral("+"), this);
  remove_btn_ = new QPushButton(QStringLiteral("-"), this);
  up_btn_ = new QPushButton(QStringLiteral("↑"), this);
  down_btn_ = new QPushButton(QStringLiteral("↓"), this);
  add_btn_->setToolTip(QStringLiteral("添加子步骤"));
  remove_btn_->setToolTip(QStringLiteral("删除选中子步骤"));
  up_btn_->setToolTip(QStringLiteral("上移选中子步骤"));
  down_btn_->setToolTip(QStringLiteral("下移选中子步骤"));
  toolbar->addWidget(add_btn_);
  toolbar->addWidget(remove_btn_);
  toolbar->addStretch();
  toolbar->addWidget(up_btn_);
  toolbar->addWidget(down_btn_);
  layout->addLayout(toolbar);

  // 表格
  table_ = new QTableWidget(0, kColCount, this);
  table_->setHorizontalHeaderLabels(
      {QStringLiteral("步骤说明"), QStringLiteral("命令"),
       QStringLiteral("目标"), QStringLiteral("值"),
       QStringLiteral("延迟(ms)")});
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->horizontalHeader()->setSectionResizeMode(kColDesc,
                                                    QHeaderView::Stretch);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->verticalHeader()->setVisible(false);
  table_->setAlternatingRowColors(true);

  // 命令列 FlatOnly 委托（子步骤不含控制流，保证不嵌套）
  auto* delegate = new CommandTypeDelegate(CommandTypeDelegate::FlatOnly, this);
  table_->setItemDelegateForColumn(kColCmd, delegate);

  layout->addWidget(table_, 1);

  updateButtonStates();
}

void SubStepTableWidget::initSignals() {
  connect(add_btn_, &QPushButton::clicked, this, &SubStepTableWidget::onAdd);
  connect(remove_btn_, &QPushButton::clicked, this,
          &SubStepTableWidget::onRemove);
  connect(up_btn_, &QPushButton::clicked, this, &SubStepTableWidget::onMoveUp);
  connect(down_btn_, &QPushButton::clicked, this,
          &SubStepTableWidget::onMoveDown);
  // 用户单元格编辑 → subStepsChanged
  connect(table_, &QTableWidget::itemChanged, this,
          [this](QTableWidgetItem*) { emit subStepsChanged(); });
  connect(table_, &QTableWidget::itemSelectionChanged, this,
          &SubStepTableWidget::updateButtonStates);
}

void SubStepTableWidget::setSubSteps(const QVector<TestStepData>& steps) {
  // 程序化填充：屏蔽 itemChanged，不发 subStepsChanged（避免切步骤/加载时
  // 误触发 dataChanged → setModified）
  QSignalBlocker blocker(table_);
  table_->setRowCount(steps.size());
  for (int i = 0; i < steps.size(); ++i) {
    const auto& s = steps[i];
    table_->setItem(i, kColDesc, new QTableWidgetItem(s.description));
    table_->setItem(i, kColCmd, new QTableWidgetItem(s.cmd));
    table_->setItem(i, kColTarget, new QTableWidgetItem(s.target));
    table_->setItem(i, kColValue, new QTableWidgetItem(s.value.toString()));
    table_->setItem(i, kColDelay,
                    new QTableWidgetItem(QString::number(s.delayMs)));
  }
  updateButtonStates();
}

QVector<TestStepData> SubStepTableWidget::subSteps() const {
  QVector<TestStepData> steps;
  for (int i = 0; i < table_->rowCount(); ++i) {
    TestStepData s;
    auto* descItem = table_->item(i, kColDesc);
    s.description = descItem ? descItem->text() : QString();
    auto* cmdItem = table_->item(i, kColCmd);
    s.cmd = cmdItem ? cmdItem->text() : QString();
    auto* targetItem = table_->item(i, kColTarget);
    s.target = targetItem ? targetItem->text() : QString();
    auto* valueItem = table_->item(i, kColValue);
    s.value = valueItem ? QVariant(valueItem->text()) : QVariant();
    auto* delayItem = table_->item(i, kColDelay);
    s.delayMs = delayItem ? delayItem->text().toInt() : 0;
    steps.append(s);
  }
  return steps;
}

void SubStepTableWidget::setReadOnly(bool ro) {
  read_only_ = ro;
  table_->setEditTriggers(
      ro ? QAbstractItemView::NoEditTriggers
         : (QAbstractItemView::DoubleClicked |
            QAbstractItemView::EditKeyPressed));
  updateButtonStates();
}

void SubStepTableWidget::onAdd() {
  QSignalBlocker blocker(table_);
  int row = table_->rowCount();
  table_->insertRow(row);
  table_->setItem(row, kColDesc, new QTableWidgetItem(QString()));
  table_->setItem(row, kColCmd, new QTableWidgetItem(QStringLiteral("SET")));
  table_->setItem(row, kColTarget, new QTableWidgetItem(QString()));
  table_->setItem(row, kColValue, new QTableWidgetItem(QString()));
  table_->setItem(row, kColDelay, new QTableWidgetItem(QStringLiteral("0")));
  table_->selectRow(row);
  emit subStepsChanged();
}

void SubStepTableWidget::onRemove() {
  int row = table_->currentRow();
  if (row < 0) {
    return;
  }
  QSignalBlocker blocker(table_);
  table_->removeRow(row);
  emit subStepsChanged();
}

void SubStepTableWidget::onMoveUp() {
  int row = table_->currentRow();
  if (row <= 0) {
    return;
  }
  swapRows(row, row - 1);
  table_->selectRow(row - 1);
  emit subStepsChanged();
}

void SubStepTableWidget::onMoveDown() {
  int row = table_->currentRow();
  if (row < 0 || row >= table_->rowCount() - 1) {
    return;
  }
  swapRows(row, row + 1);
  table_->selectRow(row + 1);
  emit subStepsChanged();
}

void SubStepTableWidget::swapRows(int rowA, int rowB) {
  QSignalBlocker blocker(table_);
  for (int c = 0; c < kColCount; ++c) {
    QString a =
        table_->item(rowA, c) ? table_->item(rowA, c)->text() : QString();
    QString b =
        table_->item(rowB, c) ? table_->item(rowB, c)->text() : QString();
    if (table_->item(rowA, c)) {
      table_->item(rowA, c)->setText(b);
    } else {
      table_->setItem(rowA, c, new QTableWidgetItem(b));
    }
    if (table_->item(rowB, c)) {
      table_->item(rowB, c)->setText(a);
    } else {
      table_->setItem(rowB, c, new QTableWidgetItem(a));
    }
  }
}

void SubStepTableWidget::updateButtonStates() {
  int row = table_->currentRow();
  int count = table_->rowCount();
  bool hasSelection = !read_only_ && row >= 0 && row < count;
  add_btn_->setEnabled(!read_only_);
  remove_btn_->setEnabled(hasSelection);
  up_btn_->setEnabled(hasSelection && row > 0);
  down_btn_->setEnabled(hasSelection && row < count - 1);
}

}  // namespace etest::app
