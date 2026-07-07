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
#include "sub_step_table_widget.h"

namespace etest::app {

StepDetailPanel::StepDetailPanel(QWidget* parent) : QWidget(parent) {
  initUi();
}

StepDetailPanel::~StepDetailPanel() = default;

void StepDetailPanel::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);

  auto* title = new QLabel(QStringLiteral("步骤详情"), this);
  title->setObjectName(QStringLiteral("stepDetailTitle"));
  layout->addWidget(title);

  stack_ = new QStackedWidget(this);
  stack_->addWidget(createEmptyPage());      // 0: Empty
  stack_->addWidget(createSetVerifyPage());  // 1: SetVerify
  stack_->addWidget(createConditionPage());  // 2: Condition
  stack_->addWidget(createLoopPage());       // 3: Loop
  stack_->addWidget(createWhilePage());      // 4: While
  stack_->addWidget(createIfPage());         // 5: If
  stack_->addWidget(createFaultPage());      // 6: Fault
  stack_->addWidget(createActionLogPage());  // 7: ActionLog
  stack_->setCurrentIndex(kPageEmpty);

  layout->addWidget(stack_, 1);
}

// ── 空页面 ──

QWidget* StepDetailPanel::createEmptyPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  empty_label_ = new QLabel(QStringLiteral("选择一个步骤以查看详情"), page);
  empty_label_->setObjectName(QStringLiteral("stepDetailEmptyLabel"));
  empty_label_->setAlignment(Qt::AlignCenter);
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
                       writePageToCache();
                       emit dataChanged();
                     }
                   });
  QObject::connect(tol_min_spin_,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   [this]() {
                     if (!internal_update_) {
                       writePageToCache();
                       emit dataChanged();
                     }
                   });
  QObject::connect(tol_max_spin_,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   [this]() {
                     if (!internal_update_) {
                       writePageToCache();
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
  cond_op_combo_->addItems({QStringLiteral("=="), QStringLiteral("!="),
                            QStringLiteral(">"), QStringLiteral("<"),
                            QStringLiteral(">="), QStringLiteral("<=")});
  form->addRow(QStringLiteral("运算符:"), cond_op_combo_);

  cond_value_edit_ = new QLineEdit(page);
  form->addRow(QStringLiteral("条件值:"), cond_value_edit_);

  QObject::connect(cond_target_edit_, &QLineEdit::textChanged, this, [this]() {
    if (!internal_update_) {
      writePageToCache();
      emit dataChanged();
    }
  });
  QObject::connect(cond_op_combo_, &QComboBox::currentTextChanged, this,
                   [this]() {
                     if (!internal_update_) {
                       writePageToCache();
                       emit dataChanged();
                     }
                   });
  QObject::connect(cond_value_edit_, &QLineEdit::textChanged, this, [this]() {
    if (!internal_update_) {
      writePageToCache();
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

  QObject::connect(loop_count_edit_, &QLineEdit::textChanged, this, [this]() {
    if (!internal_update_) {
      writePageToCache();
      emit dataChanged();
    }
  });
  QObject::connect(loop_interval_edit_, &QLineEdit::textChanged, this,
                   [this]() {
                     if (!internal_update_) {
                       writePageToCache();
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
  while_op_combo_->addItems({QStringLiteral("=="), QStringLiteral("!="),
                             QStringLiteral(">"), QStringLiteral("<"),
                             QStringLiteral(">="), QStringLiteral("<=")});
  form->addRow(QStringLiteral("运算符:"), while_op_combo_);

  while_value_edit_ = new QLineEdit(page);
  form->addRow(QStringLiteral("条件值:"), while_value_edit_);

  while_interval_edit_ = new QLineEdit(QStringLiteral("0"), page);
  form->addRow(QStringLiteral("间隔(ms):"), while_interval_edit_);

  layout->addLayout(form);

  auto* subLabel = new QLabel(QStringLiteral("循环体步骤:"), page);
  layout->addWidget(subLabel);

  while_sub_table_ = createSubStepTable(page);
  layout->addWidget(while_sub_table_, 1);

  // 连接信号
  auto connectWhile = [this]() {
    if (!internal_update_) {
      writePageToCache();
      emit dataChanged();
    }
  };
  QObject::connect(while_target_edit_, &QLineEdit::textChanged, this,
                   connectWhile);
  QObject::connect(while_op_combo_, &QComboBox::currentTextChanged, this,
                   connectWhile);
  QObject::connect(while_value_edit_, &QLineEdit::textChanged, this,
                   connectWhile);
  QObject::connect(while_interval_edit_, &QLineEdit::textChanged, this,
                   connectWhile);

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
  if_op_combo_->addItems({QStringLiteral("=="), QStringLiteral("!="),
                          QStringLiteral(">"), QStringLiteral("<"),
                          QStringLiteral(">="), QStringLiteral("<=")});
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
      writePageToCache();
      emit dataChanged();
    }
  };
  QObject::connect(if_target_edit_, &QLineEdit::textChanged, this, connectIf);
  QObject::connect(if_op_combo_, &QComboBox::currentTextChanged, this,
                   connectIf);
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

  QObject::connect(fault_type_edit_, &QLineEdit::textChanged, this, [this]() {
    if (!internal_update_) {
      writePageToCache();
      emit dataChanged();
    }
  });
  QObject::connect(fault_value_edit_, &QLineEdit::textChanged, this, [this]() {
    if (!internal_update_) {
      writePageToCache();
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

  QObject::connect(action_log_edit_, &QLineEdit::textChanged, this, [this]() {
    if (!internal_update_) {
      writePageToCache();
      emit dataChanged();
    }
  });

  return page;
}

// ── 子步骤表格 ──

SubStepTableWidget* StepDetailPanel::createSubStepTable(QWidget* parent) {
  auto* table = new SubStepTableWidget(parent);
  // 子步骤表编辑（行增删移动/单元格改动）→ 抓回 cache_ + 通知父组件把子步骤
  // 写回主行 ext data。setSubSteps 程序化填充时不发此信号，避免切步骤/加载误触发。
  connect(table, &SubStepTableWidget::subStepsChanged, this, [this]() {
    if (!internal_update_) {
      writePageToCache();
      emit dataChanged();
    }
  });
  return table;
}

// ── 公共接口 ──

// 把当前页可见的控件值抓回 cache_，避免切页后旧页数据丢失
void StepDetailPanel::writePageToCache() {
  // 即使 cache_ 的 cmd 与 current_cmd_ 不一致（极端情况），仍以 current_cmd_
  // 为准
  cache_.cmd = current_cmd_;
  int pageIdx = stack_->currentIndex();

  if (pageIdx == kPageSetVerify) {
    cache_.tolerance.enabled = tol_enable_check_->isChecked();
    cache_.tolerance.min = tol_min_spin_->value();
    cache_.tolerance.max = tol_max_spin_->value();
  } else if (pageIdx == kPageCondition) {
    cache_.condition.target = cond_target_edit_->text();
    cache_.condition.op = cond_op_combo_->currentText();
    cache_.condition.value = cond_value_edit_->text();
  } else if (pageIdx == kPageLoop) {
    cache_.loopCount = loop_count_edit_->text().toInt();
    cache_.loopIntervalMs = loop_interval_edit_->text().toInt();
    cache_.subSteps = loop_sub_table_->subSteps();
  } else if (pageIdx == kPageWhile) {
    cache_.condition.target = while_target_edit_->text();
    cache_.condition.op = while_op_combo_->currentText();
    cache_.condition.value = while_value_edit_->text();
    cache_.loopIntervalMs = while_interval_edit_->text().toInt();
    cache_.subSteps = while_sub_table_->subSteps();
  } else if (pageIdx == kPageIf) {
    cache_.condition.target = if_target_edit_->text();
    cache_.condition.op = if_op_combo_->currentText();
    cache_.condition.value = if_value_edit_->text();
    cache_.subSteps = if_then_table_->subSteps();
    cache_.elseSubSteps = if_else_table_->subSteps();
  } else if (pageIdx == kPageFault) {
    cache_.fault.type = fault_type_edit_->text();
    cache_.fault.value = fault_value_edit_->text();
  } else if (pageIdx == kPageActionLog) {
    cache_.description = action_log_edit_->text();
  }
}

// 从 cache_ 把数据写入当前页可见的控件
void StepDetailPanel::fillPageFromCache() {
  int pageIdx = stack_->currentIndex();

  if (pageIdx == kPageSetVerify) {
    tol_enable_check_->setChecked(cache_.tolerance.enabled);
    tol_min_spin_->setValue(cache_.tolerance.min);
    tol_max_spin_->setValue(cache_.tolerance.max);
  } else if (pageIdx == kPageCondition) {
    cond_target_edit_->setText(cache_.condition.target);
    int opIdx = cond_op_combo_->findText(cache_.condition.op);
    if (opIdx >= 0) {
      cond_op_combo_->setCurrentIndex(opIdx);
    }
    cond_value_edit_->setText(cache_.condition.value.toString());
  } else if (pageIdx == kPageLoop) {
    loop_count_edit_->setText(QString::number(cache_.loopCount));
    loop_interval_edit_->setText(QString::number(cache_.loopIntervalMs));
    loop_sub_table_->setSubSteps(cache_.subSteps);
  } else if (pageIdx == kPageWhile) {
    while_target_edit_->setText(cache_.condition.target);
    int opIdx = while_op_combo_->findText(cache_.condition.op);
    if (opIdx >= 0) {
      while_op_combo_->setCurrentIndex(opIdx);
    }
    while_value_edit_->setText(cache_.condition.value.toString());
    while_interval_edit_->setText(QString::number(cache_.loopIntervalMs));
    while_sub_table_->setSubSteps(cache_.subSteps);
  } else if (pageIdx == kPageIf) {
    if_target_edit_->setText(cache_.condition.target);
    int opIdx = if_op_combo_->findText(cache_.condition.op);
    if (opIdx >= 0) {
      if_op_combo_->setCurrentIndex(opIdx);
    }
    if_value_edit_->setText(cache_.condition.value.toString());
    if_then_table_->setSubSteps(cache_.subSteps);
    if_else_table_->setSubSteps(cache_.elseSubSteps);
  } else if (pageIdx == kPageFault) {
    fault_type_edit_->setText(cache_.fault.type);
    fault_value_edit_->setText(cache_.fault.value.toString());
  } else if (pageIdx == kPageActionLog) {
    action_log_edit_->setText(cache_.description);
  }
}

void StepDetailPanel::setStepData(const TestStepData& step, bool readOnly) {
  read_only_ = readOnly;
  // 在切页前先抓回当前页所有未保存的改动，防止切页时丢失
  writePageToCache();
  current_cmd_ = step.cmd.trimmed().toUpper();
  cache_ = step;  // 先把新数据整体写入 cache
  cache_.cmd = current_cmd_;
  internal_update_ = true;

  switchToPageForCommand(current_cmd_);

  // 从 cache_ 把数据写入当前页（cache_ 在前面已设为新 step）
  fillPageFromCache();

  internal_update_ = false;
}

TestStepData StepDetailPanel::stepData() const {
  // 始终从 cache_ 出发，再用当前页可见控件的"最新值"覆盖对应字段
  // （cache_ 在 setStepData / 控件 signal 链路中已更新）
  TestStepData step = cache_;
  // const 成员函数不能直接 writePageToCache，但 cache_ 在控件改动时已经更新过
  // 这里只做"当前页可能比 cache 更新"的覆盖
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
    step.subSteps = loop_sub_table_->subSteps();
  } else if (pageIdx == kPageWhile) {
    step.condition.target = while_target_edit_->text();
    step.condition.op = while_op_combo_->currentText();
    step.condition.value = while_value_edit_->text();
    step.loopIntervalMs = while_interval_edit_->text().toInt();
    step.subSteps = while_sub_table_->subSteps();
  } else if (pageIdx == kPageIf) {
    step.condition.target = if_target_edit_->text();
    step.condition.op = if_op_combo_->currentText();
    step.condition.value = if_value_edit_->text();
    step.subSteps = if_then_table_->subSteps();
    step.elseSubSteps = if_else_table_->subSteps();
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
