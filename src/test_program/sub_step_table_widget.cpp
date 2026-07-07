#include "sub_step_table_widget.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <utility>

#include "CommandTypeDelegate.h"

namespace etest::app {

namespace {
constexpr int kColDesc = 0;
constexpr int kColCmd = 1;
constexpr int kColTarget = 2;
constexpr int kColValue = 3;
constexpr int kColDelay = 4;
constexpr int kColTolerance = 5;
constexpr int kColCount = 6;

// 弹对话框编辑容差，返回新值；取消则返回原值
ToleranceSpec editTolerance(const ToleranceSpec& tol, QWidget* parent) {
  ToleranceSpec initial = tol;
  while (true) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("编辑容差"));
    auto* layout = new QFormLayout(&dlg);

    auto* enable = new QCheckBox(QStringLiteral("启用容差"), &dlg);
    enable->setChecked(initial.enabled);

    auto* minSpin = new QDoubleSpinBox(&dlg);
    minSpin->setRange(-999999.0, 999999.0);
    minSpin->setDecimals(4);
    minSpin->setValue(initial.min);
    minSpin->setEnabled(initial.enabled);

    auto* maxSpin = new QDoubleSpinBox(&dlg);
    maxSpin->setRange(-999999.0, 999999.0);
    maxSpin->setDecimals(4);
    maxSpin->setValue(initial.max);
    maxSpin->setEnabled(initial.enabled);

    QObject::connect(enable, &QCheckBox::toggled, minSpin,
                     &QWidget::setEnabled);
    QObject::connect(enable, &QCheckBox::toggled, maxSpin,
                     &QWidget::setEnabled);

    layout->addRow(enable);
    layout->addRow(QStringLiteral("容差下限:"), minSpin);
    layout->addRow(QStringLiteral("容差上限:"), maxSpin);

    auto* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
      return initial;
    }
    ToleranceSpec r;
    r.enabled = enable->isChecked();
    r.min = minSpin->value();
    r.max = maxSpin->value();
    if (r.enabled && r.min > r.max) {
      QMessageBox::warning(parent, QStringLiteral("容差"),
                           QStringLiteral("容差下限不能大于上限"));
      initial = r;
      continue;
    }
    return r;
  }
}
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
       QStringLiteral("延迟(ms)"), QStringLiteral("容差")});
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
  // 双击容差列 → 弹编辑对话框
  connect(table_, &QTableWidget::doubleClicked, this,
          &SubStepTableWidget::onDoubleClicked);
}

void SubStepTableWidget::setSubSteps(const QVector<TestStepData>& steps) {
  // 程序化填充：屏蔽 itemChanged，不发 subStepsChanged（避免切步骤/加载时
  // 误触发 dataChanged → setModified）
  QSignalBlocker blocker(table_);
  table_->setRowCount(steps.size());
  tolerances_.resize(steps.size());
  for (int i = 0; i < steps.size(); ++i) {
    const auto& s = steps[i];
    tolerances_[i] = s.tolerance;
    table_->setItem(i, kColDesc, new QTableWidgetItem(s.description));
    table_->setItem(i, kColCmd, new QTableWidgetItem(s.cmd));
    table_->setItem(i, kColTarget, new QTableWidgetItem(s.target));
    table_->setItem(i, kColValue, new QTableWidgetItem(s.value.toString()));
    table_->setItem(i, kColDelay,
                    new QTableWidgetItem(QString::number(s.delayMs)));
    // 容差列：不可编辑，显示摘要；双击弹对话框
    auto* tolItem = new QTableWidgetItem;
    tolItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    table_->setItem(i, kColTolerance, tolItem);
    refreshToleranceCell(i);
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
    s.tolerance = i < tolerances_.size() ? tolerances_[i] : ToleranceSpec{};
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
  auto* tolItem = new QTableWidgetItem;
  tolItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  table_->setItem(row, kColTolerance, tolItem);
  tolerances_.append(ToleranceSpec{});
  refreshToleranceCell(row);
  table_->selectRow(row);
  updateButtonStates();
  emit subStepsChanged();
}

void SubStepTableWidget::onRemove() {
  int row = table_->currentRow();
  if (row < 0) {
    return;
  }
  QSignalBlocker blocker(table_);
  table_->removeRow(row);
  if (row < tolerances_.size()) {
    tolerances_.removeAt(row);
  }
  updateButtonStates();
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
  // 只交换前 5 列文本，容差列由 tolerances_ 驱动刷新
  for (int c = 0; c < kColTolerance; ++c) {
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
  if (rowA >= 0 && rowA < tolerances_.size() && rowB >= 0 &&
      rowB < tolerances_.size()) {
    std::swap(tolerances_[rowA], tolerances_[rowB]);
  }
  refreshToleranceCell(rowA);
  refreshToleranceCell(rowB);
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

void SubStepTableWidget::onDoubleClicked(const QModelIndex& index) {
  if (read_only_ || !index.isValid() || index.column() != kColTolerance) {
    return;
  }
  int row = index.row();
  if (row < 0 || row >= tolerances_.size()) {
    return;
  }
  ToleranceSpec newTol = editTolerance(tolerances_[row], this);
  if (newTol.enabled == tolerances_[row].enabled &&
      newTol.min == tolerances_[row].min &&
      newTol.max == tolerances_[row].max) {
    return;  // 未改动
  }
  tolerances_[row] = newTol;
  {
    QSignalBlocker blocker(table_);  // 屏蔽 refreshToleranceCell 的 itemChanged
    refreshToleranceCell(row);
  }
  emit subStepsChanged();
}

void SubStepTableWidget::refreshToleranceCell(int row) {
  if (row < 0 || row >= table_->rowCount()) {
    return;
  }
  auto* item = table_->item(row, kColTolerance);
  if (!item) {
    return;
  }
  if (row < tolerances_.size() && tolerances_[row].enabled) {
    item->setText(QStringLiteral("%1~%2")
                      .arg(tolerances_[row].min)
                      .arg(tolerances_[row].max));
  } else {
    item->setText(QStringLiteral("—"));
  }
}

}  // namespace etest::app
