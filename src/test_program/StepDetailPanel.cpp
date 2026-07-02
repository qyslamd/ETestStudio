#include "StepDetailPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "CommandTypeDelegate.h"

namespace etest::app {

StepDetailPanel::StepDetailPanel(QWidget* parent) : QWidget(parent) {
  setupUi();
}

StepDetailPanel::~StepDetailPanel() = default;

void StepDetailPanel::setupUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);

  auto* title = new QLabel(QStringLiteral("步骤详情"), this);
  title->setStyleSheet(QStringLiteral("font-weight: bold;"));
  layout->addWidget(title);

  stack_ = new QStackedWidget(this);
  stack_->addWidget(createEmptyPage());        // 0: Empty
  stack_->addWidget(createSetVerifyPage());     // 1: SetVerify
  stack_->addWidget(createConditionPage());     // 2: Condition
  stack_->addWidget(createLoopPage());          // 3: Loop
  stack_->addWidget(createWhilePage());         // 4: While
  stack_->addWidget(createIfPage());            // 5: If
  stack_->addWidget(createFaultPage());         // 6: Fault
  stack_->addWidget(createActionLogPage());     // 7: ActionLog
  stack_->setCurrentIndex(kPageEmpty);

  layout->addWidget(stack_, 1);
}

// ── 空页面 ──

QWidget* StepDetailPanel::createEmptyPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  empty_label_ = new QLabel(QStringLiteral("选择一个步骤以查看详情"), page);
  empty_label_->setAlignment(Qt::AlignCenter);
  empty_label_->setStyleSheet(QStringLiteral("color: #888;"));
  layout->addWidget(empty_label_);
  return page;
}

// ── Set / Verify 页面（容差编辑） ──

QWidget* StepDetailPanel::createSetVerifyPage() {
  auto* page = new QWidget(this);
  auto* form = new QFormLayout(page);
  form->setSpacing(6);

  tol_enable_check_ = new QCheckBox(QStringLiteral("启用容差"), page);
  form->addRow(QStringLiteral("容差:"), tol_enable_check_);

  tol_min_spin_ = new QDoubleSpinBox(page);
  tol_min_spin_->setRange(-999999.0, 999999.0);
  tol_min_spin_->setDecimals(4);
  tol_min_spin_->setEnabled(false);
  form->addRow(QStringLiteral("容差下限:"), tol_min_spin_);

  tol_max_spin_ = new QDoubleSpinBox(page);
  tol_max_spin_->setRange(-999999.0, 999999.0);
  tol_max_spin_->setDecimals(4);
  tol_max_spin_->setEnabled(false);
  form->addRow(QStringLiteral("容差上限:"), tol_max_spin_);

  QObject::connect(tol_enable_check_, &QCheckBox::toggled, this,
                   [this](bool checked) {
                     tol_min_spin_->setEnabled(checked);
                     tol_max_spin_->setEnabled(checked);
                     if (!internal_update_) {
                       emit dataChanged();
                     }
                   });
  QObject::connect(tol_min_spin_,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   [this]() {
                     if (!internal_update_) {
                       emit dataChanged();
                     }
                   });
  QObject::connect(tol_max_spin_,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   [this]() {
                     if (!internal_update_) {
                       emit dataChanged();
                     }
                   });

  return page;
}

// ── Condition 页面（通用条件表达式） ──

QWidget* StepDetailPanel::createConditionPage() {
  auto* page = new QWidget(this);
  auto* form = new QFormLayout(page);
  form->setSpacing(6);

  cond_target_edit_ = new QLineEdit(page);
  form->addRow(QStringLiteral("条件目标:"), cond_target_edit_);

  cond_op_combo_ = new QComboBox(page);
  cond_op_combo_->addItems(
      {QStringLiteral("=="), QStringLiteral("!="), QStringLiteral(">"),
       QStringLiteral("<"), QStringLiteral(">="), QStringLiteral("<=")});
  form->addRow(QStringLiteral("运算符:"), cond_op_combo_);

  cond_value_edit_ = new QLineEdit(page);
  form->addRow(QStringLiteral("条件值:"), cond_value_edit_);

  QObject::connect(cond_target_edit_, &QLineEdit::textChanged, this,
                   [this]() {
                     if (!internal_update_) {
                       emit dataChanged();
                     }
                   });
  QObject::connect(cond_op_combo_, &QComboBox::currentTextChanged, this,
                   [this]() {
                     if (!internal_update_) {
                       emit dataChanged();
                     }
                   });
  QObject::connect(cond_value_edit_, &QLineEdit::textChanged, this,
                   [this]() {
                     if (!internal_update_) {
                       emit dataChanged();
                     }
                   });

  return page;
}

// ── Loop 页面 ──

QWidget* StepDetailPanel::createLoopPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setSpacing(4);

  auto* form = new QFormLayout();
  form->setSpacing(4);

  loop_count_edit_ = new QLineEdit(QStringLiteral("1"), page);
  form->addRow(QStringLiteral("循环次数:"), loop_count_edit_);

  loop_interval_edit_ = new QLineEdit(QStringLiteral("0"), page);
  form->addRow(QStringLiteral("间隔(ms):"), loop_interval_edit_);

  layout->addLayout(form);

  auto* subLabel = new QLabel(QStringLiteral("循环体步骤:"), page);
  layout->addWidget(subLabel);

  loop_sub_table_ = createSubStepTable(page);
  layout->addWidget(loop_sub_table_, 1);

  QObject::connect(loop_count_edit_, &QLineEdit::textChanged, this,
                   [this]() {
                     if (!internal_update_) {
                       emit dataChanged();
                     }
                   });
  QObject::connect(loop_interval_edit_, &QLineEdit::textChanged, this,
                   [this]() {
                     if (!internal_update_) {
                       emit dataChanged();
                     }
                   });

  return page;
}

// ── While 页面 ──

QWidget* StepDetailPanel::createWhilePage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setSpacing(4);

  auto* form = new QFormLayout();
  form->setSpacing(4);

  while_target_edit_ = new QLineEdit(page);
  form->addRow(QStringLiteral("条件目标:"), while_target_edit_);

  while_op_combo_ = new QComboBox(page);
  while_op_combo_->addItems(
      {QStringLiteral("=="), QStringLiteral("!="), QStringLiteral(">"),
       QStringLiteral("<"), QStringLiteral(">="), QStringLiteral("<=")});
  form->addRow(QStringLiteral("运算符:"), while_op_combo_);

  while_value_edit_ = new QLineEdit(page);
  form->addRow(QStringLiteral("条件值:"), while_value_edit_);

  while_interval_edit_ = new QLineEdit(QStringLiteral("0"), page);
  form->addRow(QStringLiteral("间隔(ms):"), while_interval_edit_);

  while_timeout_edit_ = new QLineEdit(QStringLiteral("30000"), page);
  form->addRow(QStringLiteral("超时(ms):"), while_timeout_edit_);

  layout->addLayout(form);

  auto* subLabel = new QLabel(QStringLiteral("循环体步骤:"), page);
  layout->addWidget(subLabel);

  while_sub_table_ = createSubStepTable(page);
  layout->addWidget(while_sub_table_, 1);

  // 连接信号
  auto connectWhile = [this]() {
    if (!internal_update_) {
      emit dataChanged();
    }
  };
  QObject::connect(while_target_edit_, &QLineEdit::textChanged, this, connectWhile);
  QObject::connect(while_op_combo_, &QComboBox::currentTextChanged, this, connectWhile);
  QObject::connect(while_value_edit_, &QLineEdit::textChanged, this, connectWhile);
  QObject::connect(while_interval_edit_, &QLineEdit::textChanged, this, connectWhile);
  QObject::connect(while_timeout_edit_, &QLineEdit::textChanged, this, connectWhile);

  return page;
}

// ── If 页面 ──

QWidget* StepDetailPanel::createIfPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setSpacing(4);

  auto* form = new QFormLayout();
  form->setSpacing(4);

  if_target_edit_ = new QLineEdit(page);
  form->addRow(QStringLiteral("条件目标:"), if_target_edit_);

  if_op_combo_ = new QComboBox(page);
  if_op_combo_->addItems(
      {QStringLiteral("=="), QStringLiteral("!="), QStringLiteral(">"),
       QStringLiteral("<"), QStringLiteral(">="), QStringLiteral("<=")});
  form->addRow(QStringLiteral("运算符:"), if_op_combo_);

  if_value_edit_ = new QLineEdit(page);
  form->addRow(QStringLiteral("条件值:"), if_value_edit_);

  layout->addLayout(form);

  auto* thenLabel = new QLabel(QStringLiteral("Then 分支:"), page);
  layout->addWidget(thenLabel);

  if_then_table_ = createSubStepTable(page);
  layout->addWidget(if_then_table_, 1);

  auto* elseLabel = new QLabel(QStringLiteral("Else 分支（可选）:"), page);
  layout->addWidget(elseLabel);

  if_else_table_ = createSubStepTable(page);
  layout->addWidget(if_else_table_, 1);

  auto connectIf = [this]() {
    if (!internal_update_) {
      emit dataChanged();
    }
  };
  QObject::connect(if_target_edit_, &QLineEdit::textChanged, this, connectIf);
  QObject::connect(if_op_combo_, &QComboBox::currentTextChanged, this, connectIf);
  QObject::connect(if_value_edit_, &QLineEdit::textChanged, this, connectIf);

  return page;
}

// ── Fault 页面 ──

QWidget* StepDetailPanel::createFaultPage() {
  auto* page = new QWidget(this);
  auto* form = new QFormLayout(page);
  form->setSpacing(6);

  fault_type_edit_ = new QLineEdit(page);
  fault_type_edit_->setPlaceholderText(
      QStringLiteral("stuck_at / offset / noise / crc_error"));
  form->addRow(QStringLiteral("故障类型:"), fault_type_edit_);

  fault_value_edit_ = new QLineEdit(page);
  form->addRow(QStringLiteral("故障值:"), fault_value_edit_);

  QObject::connect(fault_type_edit_, &QLineEdit::textChanged, this,
                   [this]() {
                     if (!internal_update_) {
                       emit dataChanged();
                     }
                   });
  QObject::connect(fault_value_edit_, &QLineEdit::textChanged, this,
                   [this]() {
                     if (!internal_update_) {
                       emit dataChanged();
                     }
                   });

  return page;
}

// ── Action/Log 页面 ──

QWidget* StepDetailPanel::createActionLogPage() {
  auto* page = new QWidget(this);
  auto* form = new QFormLayout(page);
  form->setSpacing(6);

  action_log_edit_ = new QLineEdit(page);
  form->addRow(QStringLiteral("内容:"), action_log_edit_);

  QObject::connect(action_log_edit_, &QLineEdit::textChanged, this,
                   [this]() {
                     if (!internal_update_) {
                       emit dataChanged();
                     }
                   });

  return page;
}

// ── 子步骤表格 ──

QTableWidget* StepDetailPanel::createSubStepTable(QWidget* parent) {
  auto* table = new QTableWidget(0, 5, parent);
  table->setHorizontalHeaderLabels(
      {QStringLiteral("步骤说明"), QStringLiteral("命令"),
       QStringLiteral("目标"), QStringLiteral("值"),
       QStringLiteral("延迟(ms)")});
  table->horizontalHeader()->setStretchLastSection(true);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->verticalHeader()->setVisible(false);
  table->setAlternatingRowColors(true);

  // FlatOnly delegate
  auto* delegate = new CommandTypeDelegate(CommandTypeDelegate::FlatOnly, this);
  table->setItemDelegateForColumn(1, delegate);

  return table;
}

// ── 公共接口 ──

void StepDetailPanel::setStepData(const TestStepData& step, bool readOnly) {
  read_only_ = readOnly;
  current_cmd_ = step.cmd.trimmed().toUpper();
  internal_update_ = true;

  switchToPageForCommand(current_cmd_);

  // 根据页面类型填充数据
  int pageIdx = stack_->currentIndex();

  if (pageIdx == kPageSetVerify) {
    // VERIFY tolerance
    tol_enable_check_->setChecked(step.tolerance.enabled);
    tol_min_spin_->setValue(step.tolerance.min);
    tol_max_spin_->setValue(step.tolerance.max);
  } else if (pageIdx == kPageCondition) {
    // WAIT condition
    cond_target_edit_->setText(step.condition.target);
    int opIdx = cond_op_combo_->findText(step.condition.op);
    if (opIdx >= 0) {
      cond_op_combo_->setCurrentIndex(opIdx);
    }
    cond_value_edit_->setText(step.condition.value.toString());
  } else if (pageIdx == kPageLoop) {
    loop_count_edit_->setText(QString::number(step.loopCount));
    loop_interval_edit_->setText(QString::number(step.loopIntervalMs));
    loop_sub_table_->setRowCount(step.subSteps.size());
    for (int i = 0; i < step.subSteps.size(); ++i) {
      const auto& s = step.subSteps[i];
      loop_sub_table_->setItem(i, 0, new QTableWidgetItem(s.description));
      loop_sub_table_->setItem(i, 1, new QTableWidgetItem(s.cmd));
      loop_sub_table_->setItem(i, 2, new QTableWidgetItem(s.target));
      loop_sub_table_->setItem(i, 3, new QTableWidgetItem(s.value.toString()));
      loop_sub_table_->setItem(
          i, 4, new QTableWidgetItem(QString::number(s.delayMs)));
    }
  } else if (pageIdx == kPageWhile) {
    while_target_edit_->setText(step.condition.target);
    int opIdx = while_op_combo_->findText(step.condition.op);
    if (opIdx >= 0) {
      while_op_combo_->setCurrentIndex(opIdx);
    }
    while_value_edit_->setText(step.condition.value.toString());
    while_interval_edit_->setText(QString::number(step.loopIntervalMs));
    while_timeout_edit_->setText(QString::number(step.timeoutMs));
    while_sub_table_->setRowCount(step.subSteps.size());
    for (int i = 0; i < step.subSteps.size(); ++i) {
      const auto& s = step.subSteps[i];
      while_sub_table_->setItem(i, 0, new QTableWidgetItem(s.description));
      while_sub_table_->setItem(i, 1, new QTableWidgetItem(s.cmd));
      while_sub_table_->setItem(i, 2, new QTableWidgetItem(s.target));
      while_sub_table_->setItem(i, 3, new QTableWidgetItem(s.value.toString()));
      while_sub_table_->setItem(
          i, 4, new QTableWidgetItem(QString::number(s.delayMs)));
    }
  } else if (pageIdx == kPageIf) {
    if_target_edit_->setText(step.condition.target);
    int opIdx = if_op_combo_->findText(step.condition.op);
    if (opIdx >= 0) {
      if_op_combo_->setCurrentIndex(opIdx);
    }
    if_value_edit_->setText(step.condition.value.toString());
    // Then
    if_then_table_->setRowCount(step.subSteps.size());
    for (int i = 0; i < step.subSteps.size(); ++i) {
      const auto& s = step.subSteps[i];
      if_then_table_->setItem(i, 0, new QTableWidgetItem(s.description));
      if_then_table_->setItem(i, 1, new QTableWidgetItem(s.cmd));
      if_then_table_->setItem(i, 2, new QTableWidgetItem(s.target));
      if_then_table_->setItem(i, 3, new QTableWidgetItem(s.value.toString()));
      if_then_table_->setItem(
          i, 4, new QTableWidgetItem(QString::number(s.delayMs)));
    }
    // Else
    if_else_table_->setRowCount(step.elseSubSteps.size());
    for (int i = 0; i < step.elseSubSteps.size(); ++i) {
      const auto& s = step.elseSubSteps[i];
      if_else_table_->setItem(i, 0, new QTableWidgetItem(s.description));
      if_else_table_->setItem(i, 1, new QTableWidgetItem(s.cmd));
      if_else_table_->setItem(i, 2, new QTableWidgetItem(s.target));
      if_else_table_->setItem(i, 3, new QTableWidgetItem(s.value.toString()));
      if_else_table_->setItem(
          i, 4, new QTableWidgetItem(QString::number(s.delayMs)));
    }
  } else if (pageIdx == kPageFault) {
    fault_type_edit_->setText(step.fault.type);
    fault_value_edit_->setText(step.fault.value.toString());
  } else if (pageIdx == kPageActionLog) {
    action_log_edit_->setText(step.description);
  }

  internal_update_ = false;
}

TestStepData StepDetailPanel::stepData() const {
  TestStepData step;
  step.cmd = current_cmd_;

  int pageIdx = stack_->currentIndex();

  if (pageIdx == kPageSetVerify) {
    step.tolerance.enabled = tol_enable_check_->isChecked();
    step.tolerance.min = tol_min_spin_->value();
    step.tolerance.max = tol_max_spin_->value();
  } else if (pageIdx == kPageCondition) {
    step.condition.target = cond_target_edit_->text();
    step.condition.op = cond_op_combo_->currentText();
    step.condition.value = cond_value_edit_->text();
  } else if (pageIdx == kPageLoop) {
    step.loopCount = loop_count_edit_->text().toInt();
    step.loopIntervalMs = loop_interval_edit_->text().toInt();
    for (int i = 0; i < loop_sub_table_->rowCount(); ++i) {
      TestStepData s;
      s.description =
          loop_sub_table_->item(i, 0) ? loop_sub_table_->item(i, 0)->text() : QString();
      s.cmd = loop_sub_table_->item(i, 1) ? loop_sub_table_->item(i, 1)->text() : QString();
      s.target = loop_sub_table_->item(i, 2) ? loop_sub_table_->item(i, 2)->text() : QString();
      s.value = loop_sub_table_->item(i, 3)
                    ? QVariant(loop_sub_table_->item(i, 3)->text())
                    : QVariant();
      s.delayMs =
          loop_sub_table_->item(i, 4) ? loop_sub_table_->item(i, 4)->text().toInt() : 0;
      step.subSteps.append(s);
    }
  } else if (pageIdx == kPageWhile) {
    step.condition.target = while_target_edit_->text();
    step.condition.op = while_op_combo_->currentText();
    step.condition.value = while_value_edit_->text();
    step.loopIntervalMs = while_interval_edit_->text().toInt();
    step.timeoutMs = while_timeout_edit_->text().toInt();
    for (int i = 0; i < while_sub_table_->rowCount(); ++i) {
      TestStepData s;
      s.description =
          while_sub_table_->item(i, 0) ? while_sub_table_->item(i, 0)->text() : QString();
      s.cmd = while_sub_table_->item(i, 1) ? while_sub_table_->item(i, 1)->text() : QString();
      s.target = while_sub_table_->item(i, 2) ? while_sub_table_->item(i, 2)->text() : QString();
      s.value = while_sub_table_->item(i, 3)
                    ? QVariant(while_sub_table_->item(i, 3)->text())
                    : QVariant();
      s.delayMs =
          while_sub_table_->item(i, 4) ? while_sub_table_->item(i, 4)->text().toInt() : 0;
      step.subSteps.append(s);
    }
  } else if (pageIdx == kPageIf) {
    step.condition.target = if_target_edit_->text();
    step.condition.op = if_op_combo_->currentText();
    step.condition.value = if_value_edit_->text();
    for (int i = 0; i < if_then_table_->rowCount(); ++i) {
      TestStepData s;
      s.description =
          if_then_table_->item(i, 0) ? if_then_table_->item(i, 0)->text() : QString();
      s.cmd = if_then_table_->item(i, 1) ? if_then_table_->item(i, 1)->text() : QString();
      s.target = if_then_table_->item(i, 2) ? if_then_table_->item(i, 2)->text() : QString();
      s.value = if_then_table_->item(i, 3)
                    ? QVariant(if_then_table_->item(i, 3)->text())
                    : QVariant();
      s.delayMs =
          if_then_table_->item(i, 4) ? if_then_table_->item(i, 4)->text().toInt() : 0;
      step.subSteps.append(s);
    }
    for (int i = 0; i < if_else_table_->rowCount(); ++i) {
      TestStepData s;
      s.description =
          if_else_table_->item(i, 0) ? if_else_table_->item(i, 0)->text() : QString();
      s.cmd = if_else_table_->item(i, 1) ? if_else_table_->item(i, 1)->text() : QString();
      s.target = if_else_table_->item(i, 2) ? if_else_table_->item(i, 2)->text() : QString();
      s.value = if_else_table_->item(i, 3)
                    ? QVariant(if_else_table_->item(i, 3)->text())
                    : QVariant();
      s.delayMs =
          if_else_table_->item(i, 4) ? if_else_table_->item(i, 4)->text().toInt() : 0;
      step.elseSubSteps.append(s);
    }
  } else if (pageIdx == kPageFault) {
    step.fault.type = fault_type_edit_->text();
    step.fault.value = fault_value_edit_->text();
  } else if (pageIdx == kPageActionLog) {
    step.description = action_log_edit_->text();
  }

  return step;
}

void StepDetailPanel::clear() {
  internal_update_ = true;
  stack_->setCurrentIndex(kPageEmpty);
  current_cmd_.clear();
  internal_update_ = false;
}

void StepDetailPanel::switchToPageForCommand(const QString& cmd) {
  if (cmd == QStringLiteral("SET") || cmd == QStringLiteral("VERIFY")) {
    stack_->setCurrentIndex(kPageSetVerify);
  } else if (cmd == QStringLiteral("WAIT")) {
    stack_->setCurrentIndex(kPageCondition);
  } else if (cmd == QStringLiteral("LOOP")) {
    stack_->setCurrentIndex(kPageLoop);
  } else if (cmd == QStringLiteral("WHILE")) {
    stack_->setCurrentIndex(kPageWhile);
  } else if (cmd == QStringLiteral("IF")) {
    stack_->setCurrentIndex(kPageIf);
  } else if (cmd == QStringLiteral("INJECT_FAULT")) {
    stack_->setCurrentIndex(kPageFault);
  } else if (cmd == QStringLiteral("ACTION") || cmd == QStringLiteral("LOG")) {
    stack_->setCurrentIndex(kPageActionLog);
  } else {
    stack_->setCurrentIndex(kPageEmpty);
  }
}

}  // namespace etest::app
