#include "StepEditDialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace etest::app {

namespace {
const char* const kCmdList[] = {"SET",   "CHECK", "VERIFY", "WAIT",
                                "DELAY", "LOOP",  "WHILE",  "IF"};

// 目标信号：SET/CHECK/VERIFY/WAIT/WHILE/IF
bool targetVisible(const QString& cmd) {
  return cmd != QStringLiteral("DELAY") && cmd != QStringLiteral("LOOP");
}
// 值：SET/CHECK/VERIFY/WAIT/DELAY
bool valueVisible(const QString& cmd) {
  return cmd != QStringLiteral("LOOP") && cmd != QStringLiteral("WHILE") &&
         cmd != QStringLiteral("IF");
}
// 容差：SET/CHECK/VERIFY
bool toleranceVisible(const QString& cmd) {
  return cmd == QStringLiteral("SET") || cmd == QStringLiteral("CHECK") ||
         cmd == QStringLiteral("VERIFY");
}
// 超时：VERIFY/WAIT/WHILE
bool timeoutVisible(const QString& cmd) {
  return cmd == QStringLiteral("VERIFY") || cmd == QStringLiteral("WAIT") ||
         cmd == QStringLiteral("WHILE");
}
// 条件运算符：WAIT/WHILE/IF
bool conditionVisible(const QString& cmd) {
  return cmd == QStringLiteral("WAIT") || cmd == QStringLiteral("WHILE") ||
         cmd == QStringLiteral("IF");
}
// 循环次数：LOOP/WHILE
bool loopVisible(const QString& cmd) {
  return cmd == QStringLiteral("LOOP") || cmd == QStringLiteral("WHILE");
}
bool isControlFlow(const QString& cmd) {
  return cmd == QStringLiteral("LOOP") || cmd == QStringLiteral("WHILE") ||
         cmd == QStringLiteral("IF");
}

QLabel* makeFieldLabel(QWidget* parent, const QString& text) {
  auto* label = new QLabel(text, parent);
  label->setFixedWidth(76);
  return label;
}

QWidget* makeFieldRow(QWidget* parent, const QString& labelText,
                      QWidget* field) {
  auto* row = new QWidget(parent);
  auto* lay = new QHBoxLayout(row);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(10);
  lay->addWidget(makeFieldLabel(row, labelText));
  lay->addWidget(field, 1);
  return row;
}
}  // namespace

StepEditDialog::StepEditDialog(QWidget* parent) : OverlayDialog(parent) {
  initUi();
  initSignals();
}

void StepEditDialog::initUi() {
  setWindowTitle(QStringLiteral("添加步骤"));

  auto* card = new QWidget(this);
  card->setObjectName(QStringLiteral("stepModal"));
  card->setMinimumWidth(560);

  auto* lay = new QVBoxLayout(card);
  lay->setContentsMargins(24, 20, 24, 20);
  lay->setSpacing(14);

  auto* title = new QLabel(QStringLiteral("添加步骤"), card);
  title->setObjectName(QStringLiteral("stepModalTitle"));
  lay->addWidget(title);

  // 命令类型
  cmd_combo_ = new QComboBox(card);
  for (const char* c : kCmdList) {
    cmd_combo_->addItem(QString::fromLatin1(c));
  }
  lay->addWidget(makeFieldRow(card, QStringLiteral("命令类型"), cmd_combo_));

  // 目标信号
  target_edit_ = new QLineEdit(card);
  target_edit_->setPlaceholderText(QStringLiteral("信号或变量名称"));
  target_row_ = makeFieldRow(card, QStringLiteral("目标信号"), target_edit_);
  lay->addWidget(target_row_);

  // 值 + 容差
  value_edit_ = new QLineEdit(card);
  tolerance_edit_ = new QLineEdit(card);
  value_row_ = new QWidget(card);
  auto* valueLay = new QHBoxLayout(value_row_);
  valueLay->setContentsMargins(0, 0, 0, 0);
  valueLay->setSpacing(10);
  valueLay->addWidget(makeFieldLabel(value_row_, QStringLiteral("值")));
  valueLay->addWidget(value_edit_, 1);
  tolerance_col_ = new QWidget(value_row_);
  auto* tolLay = new QHBoxLayout(tolerance_col_);
  tolLay->setContentsMargins(0, 0, 0, 0);
  tolLay->setSpacing(10);
  tolLay->addWidget(makeFieldLabel(tolerance_col_, QStringLiteral("容差")));
  tolLay->addWidget(tolerance_edit_);
  valueLay->addWidget(tolerance_col_);
  lay->addWidget(value_row_);

  // 超时 + 条件
  timeout_edit_ = new QLineEdit(card);
  condition_combo_ = new QComboBox(card);
  condition_combo_->addItems({QStringLiteral("=="), QStringLiteral("!="),
                              QStringLiteral(">"), QStringLiteral("<"),
                              QStringLiteral(">="), QStringLiteral("<=")});
  extra_row_ = new QWidget(card);
  auto* extraLay = new QHBoxLayout(extra_row_);
  extraLay->setContentsMargins(0, 0, 0, 0);
  extraLay->setSpacing(10);
  timeout_col_ = new QWidget(extra_row_);
  auto* timeoutLay = new QHBoxLayout(timeout_col_);
  timeoutLay->setContentsMargins(0, 0, 0, 0);
  timeoutLay->setSpacing(10);
  timeoutLay->addWidget(makeFieldLabel(timeout_col_, QStringLiteral("超时(ms)")));
  timeoutLay->addWidget(timeout_edit_, 1);
  extraLay->addWidget(timeout_col_, 1);
  condition_col_ = new QWidget(extra_row_);
  auto* condLay = new QHBoxLayout(condition_col_);
  condLay->setContentsMargins(0, 0, 0, 0);
  condLay->setSpacing(10);
  condLay->addWidget(makeFieldLabel(condition_col_, QStringLiteral("条件")));
  condLay->addWidget(condition_combo_, 1);
  extraLay->addWidget(condition_col_, 1);
  lay->addWidget(extra_row_);

  // 循环次数
  loop_count_edit_ = new QLineEdit(card);
  loop_row_ = makeFieldRow(card, QStringLiteral("循环次数"), loop_count_edit_);
  lay->addWidget(loop_row_);

  // IF 的 ELSE 分支
  else_row_ = new QWidget(card);
  auto* elseLay = new QHBoxLayout(else_row_);
  elseLay->setContentsMargins(0, 0, 0, 0);
  else_checkbox_ = new QCheckBox(QStringLiteral("包含 ELSE 分支"), else_row_);
  elseLay->addWidget(else_checkbox_);
  elseLay->addStretch();
  lay->addWidget(else_row_);

  lay->addStretch();

  // 按钮行
  auto* btnRow = new QHBoxLayout();
  btnRow->setSpacing(10);
  btnRow->addStretch();
  cancel_btn_ = new QPushButton(QStringLiteral("取消"), card);
  cancel_btn_->setObjectName(QStringLiteral("stepCancelBtn"));
  confirm_btn_ = new QPushButton(QStringLiteral("添加步骤"), card);
  confirm_btn_->setObjectName(QStringLiteral("stepConfirmBtn"));
  confirm_btn_->setDefault(true);
  btnRow->addWidget(cancel_btn_);
  btnRow->addWidget(confirm_btn_);
  lay->addLayout(btnRow);

  setWidget(card);

  // 初始可见性：默认 SET
  onCmdChanged(currentCmd());
}

void StepEditDialog::initSignals() {
  connect(cmd_combo_, &QComboBox::currentTextChanged, this,
          &StepEditDialog::onCmdChanged);
  connect(confirm_btn_, &QPushButton::clicked, this, [this]() {
    if (validateInputs()) {
      accept();
    }
  });
  connect(cancel_btn_, &QPushButton::clicked, this, &StepEditDialog::reject);
}

void StepEditDialog::configure(bool editing, bool insideBlock,
                               const StepEditResult& initial) {
  // 重建命令项以重置使能态（块体内禁用 LOOP/WHILE/IF）
  cmd_combo_->blockSignals(true);
  cmd_combo_->clear();
  for (int i = 0; i < 8; ++i) {
    cmd_combo_->addItem(QString::fromLatin1(kCmdList[i]));
    if (insideBlock && isControlFlow(QString::fromLatin1(kCmdList[i]))) {
      if (auto* model =
              qobject_cast<QStandardItemModel*>(cmd_combo_->model())) {
        model->item(i)->setEnabled(false);
      }
    }
  }
  const int idx = cmd_combo_->findText(initial.cmd);
  cmd_combo_->setCurrentIndex(idx < 0 ? 0 : idx);
  cmd_combo_->blockSignals(false);

  // 预填
  target_edit_->setText(initial.target);
  value_edit_->setText(initial.value);
  tolerance_edit_->setText(initial.tolerance);
  timeout_edit_->setText(initial.timeout);
  const int condIdx = condition_combo_->findText(initial.condition);
  condition_combo_->setCurrentIndex(condIdx < 0 ? 0 : condIdx);
  loop_count_edit_->setText(initial.loopCount);
  else_checkbox_->setChecked(initial.includeElse);

  confirm_btn_->setText(editing ? QStringLiteral("保存修改")
                                : QStringLiteral("添加步骤"));
  setWindowTitle(editing ? QStringLiteral("编辑步骤")
                         : QStringLiteral("添加步骤"));

  onCmdChanged(currentCmd());
}

StepEditResult StepEditDialog::result() const {
  StepEditResult r;
  r.cmd = currentCmd();
  r.target = target_edit_->text().trimmed();
  r.value = value_edit_->text().trimmed();
  r.tolerance = tolerance_edit_->text().trimmed();
  r.timeout = timeout_edit_->text().trimmed();
  r.condition = condition_combo_->currentText();
  r.loopCount = loop_count_edit_->text().trimmed();
  r.includeElse = else_checkbox_->isChecked();
  return r;
}

void StepEditDialog::onCmdChanged(const QString& cmd) {
  target_row_->setVisible(targetVisible(cmd));
  value_row_->setVisible(valueVisible(cmd));
  tolerance_col_->setVisible(toleranceVisible(cmd));
  timeout_col_->setVisible(timeoutVisible(cmd));
  condition_col_->setVisible(conditionVisible(cmd));
  loop_row_->setVisible(loopVisible(cmd));
  else_row_->setVisible(cmd == QStringLiteral("IF"));
  extra_row_->setVisible(timeoutVisible(cmd) || conditionVisible(cmd));
  // DELAY 的值为延时毫秒
  value_edit_->setPlaceholderText(
      cmd == QStringLiteral("DELAY") ? QStringLiteral("例如：100（毫秒）")
                                     : QStringLiteral("例如：5.0"));
}

// 数值字段校验：loopCount/超时/DELAY 值须可解析，避免静默钳制为缺省值
bool StepEditDialog::validateInputs() {
  const QString cmd = currentCmd();
  if (targetVisible(cmd) && target_edit_->text().trimmed().isEmpty()) {
    target_edit_->setFocus();
    return false;
  }
  if (valueVisible(cmd) && value_edit_->text().trimmed().isEmpty()) {
    value_edit_->setFocus();
    return false;
  }
  if (loopVisible(cmd)) {
    const QString text = loop_count_edit_->text().trimmed();
    bool ok = false;
    const int count = text.toInt(&ok);
    if (text.isEmpty() || !ok || count <= 0) {
      loop_count_edit_->setFocus();
      return false;
    }
  }
  if (timeoutVisible(cmd)) {
    // 空超时允许：翻译层按缺省 5000 兜底；非数字则拒绝
    const QString text = timeout_edit_->text().trimmed();
    if (!text.isEmpty()) {
      bool ok = false;
      text.toInt(&ok);
      if (!ok) {
        timeout_edit_->setFocus();
        return false;
      }
    }
  }
  if (cmd == QStringLiteral("DELAY")) {
    // delayMs 为整数，与翻译层 toInt 口径一致（避免科学计数/小数被截断）
    const QString text = value_edit_->text().trimmed();
    bool ok = false;
    const int delay = text.toInt(&ok);
    if (text.isEmpty() || !ok || delay < 0) {
      value_edit_->setFocus();
      return false;
    }
  }
  return true;
}

QString StepEditDialog::currentCmd() const {
  return cmd_combo_->currentText();
}

void StepEditDialog::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    event->accept();
    return;
  }
  if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
      !qobject_cast<QAbstractButton*>(QApplication::focusWidget())) {
    if (validateInputs()) {
      accept();
    }
    event->accept();
    return;
  }
  OverlayDialog::keyPressEvent(event);
}

}  // namespace etest::app
